/*
   Copyright (c) 2026 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "SessionServiceAdapter.h"

#include "SessionCodecs.h"

#include <algorithm>
#include <condition_variable>
#include <deque>
#include <map>
#include <mutex>
#include <sstream>
#include <thread>
#include <unordered_map>

namespace knobkraft::recall {
	using namespace midikraft::session;

	namespace {
		template<typename T> ServiceResult<ServiceResponse<T>> failure(ServiceError error) {
			return ServiceResult<ServiceResponse<T>>::failure(std::move(error));
		}

		template<typename T> ServiceResult<ServiceResponse<T>> response(std::string requestId, T value) {
			return ServiceResult<ServiceResponse<T>>::success({ std::move(requestId), std::move(value) });
		}

		SessionSynthInfo project(ConfiguredSynthInstance const& source) {
			return { source.instanceId, source.displayName, source.adaptationId, source.online,
				{ source.capabilities.editBuffer, source.capabilities.programDump,
				  source.capabilities.customProgramChange, source.capabilities.verification } };
		}

		SessionPatch project(EnginePatch const& source) {
			SessionPatch result;
			result.adaptationId = source.adaptationId;
			result.dataTypeId = source.dataTypeId;
			result.name = source.name;
			result.payload = source.payload;
			result.source = source.source;
			auto fingerprint = SessionPatchCodec::fingerprint(result);
			if (fingerprint) result.fingerprint = fingerprint.value();
			return result;
		}

		std::optional<std::size_t> pageOffset(std::optional<std::string> const& token) {
			if (!token) return 0;
			try {
				std::size_t consumed = 0;
				auto value = std::stoull(*token, &consumed);
				if (consumed != token->size()) return std::nullopt;
				return static_cast<std::size_t>(value);
			}
			catch (...) { return std::nullopt; }
		}
	}

	struct SessionServiceAdapter::Impl {
		struct Job {
			ApplyToEditBufferRequest request;
			std::string transferId;
			std::atomic_bool cancelled { false };
		};
		struct DeviceQueue {
			std::mutex mutex;
			std::condition_variable changed;
			std::deque<std::shared_ptr<Job>> jobs;
			bool stopping = false;
			std::thread worker;
		};

		EngineSessionBackend& backend;
		SessionServiceAdapterConfig config;
		mutable std::mutex stateMutex;
		SessionSnapshot snapshot;
		std::map<std::string, TransferRecord> transfers;
		std::map<std::string, std::string> requestToTransfer;
		std::map<std::string, std::shared_ptr<Job>> jobsByTransfer;
		std::map<std::string, std::unique_ptr<DeviceQueue>> deviceQueues;
		std::map<ObserverId, SessionObserver> observers;
		ObserverId nextObserver = 1;

		Impl(EngineSessionBackend& backendIn, SessionServiceAdapterConfig configIn)
			: backend(backendIn), config(std::move(configIn)) {
			if (!config.nowUnixMillis) config.nowUnixMillis = [] {
				return std::chrono::duration_cast<std::chrono::milliseconds>(
					std::chrono::system_clock::now().time_since_epoch()).count();
			};
			if (!config.transferIdGenerator) {
				config.transferIdGenerator = [counter = std::uint64_t { 0 }]() mutable {
					return "recall-transfer-" + std::to_string(++counter);
				};
			}
		}

		~Impl() {
			std::vector<DeviceQueue*> queues;
			{
				std::lock_guard lock(stateMutex);
				for (auto const& [id, job] : jobsByTransfer) {
					(void)id;
					job->cancelled.store(true);
				}
				for (auto& [id, queue] : deviceQueues) {
					(void)id;
					queues.push_back(queue.get());
					std::lock_guard queueLock(queue->mutex);
					queue->stopping = true;
				}
			}
			for (auto* queue : queues) queue->changed.notify_all();
			for (auto* queue : queues) if (queue->worker.joinable()) queue->worker.join();
		}

		std::optional<ServiceError> validate(RequestContext const& context) const {
			if (context.requestId.empty()) return ServiceError { ServiceErrorCode::InvalidRequest, "requestId is required", false };
			if (context.deadlineUnixMillis && *context.deadlineUnixMillis <= config.nowUnixMillis())
				return ServiceError { ServiceErrorCode::DeadlineExceeded, "request deadline has elapsed", true };
			return std::nullopt;
		}

		std::optional<ConfiguredSynthInstance> findSynth(std::string const& id) {
			auto all = backend.configuredSynths();
			auto found = std::find_if(all.begin(), all.end(), [&](auto const& item) { return item.instanceId == id; });
			return found == all.end() ? std::nullopt : std::optional<ConfiguredSynthInstance>(*found);
		}

		void rebuildSnapshotLocked() {
			snapshot.transfers.clear();
			for (auto const& [id, transfer] : transfers) {
				(void)id;
				snapshot.transfers.push_back(transfer);
			}
			++snapshot.revision;
		}

		void emitSnapshot() {
			SessionSnapshot copy;
			std::vector<SessionObserver> listeners;
			{
				std::lock_guard lock(stateMutex);
				rebuildSnapshotLocked();
				copy = snapshot;
				for (auto const& [id, observer] : observers) { (void)id; listeners.push_back(observer); }
			}
			for (auto const& observer : listeners) if (observer) observer(copy);
		}

		void updateTransfer(std::string const& id, std::function<void(TransferRecord&)> update) {
			{
				std::lock_guard lock(stateMutex);
				auto found = transfers.find(id);
				if (found == transfers.end()) return;
				update(found->second);
				found->second.updatedAtUnixMillis = config.nowUnixMillis();
			}
			emitSnapshot();
		}

		void process(DeviceQueue& queue) {
			for (;;) {
				std::shared_ptr<Job> job;
				{
					std::unique_lock lock(queue.mutex);
					queue.changed.wait(lock, [&] { return queue.stopping || !queue.jobs.empty(); });
					if (queue.stopping && queue.jobs.empty()) return;
					job = queue.jobs.front();
					queue.jobs.pop_front();
				}
				if (job->cancelled.load()) {
					updateTransfer(job->transferId, [](auto& record) {
						record.status.state = TransferState::Cancelled;
						record.status.detail = "Cancelled before sending";
						record.error = ServiceError { ServiceErrorCode::TransferCancelled, "Transfer cancelled", false };
					});
					continue;
				}
				if (job->request.context.deadlineUnixMillis
					&& *job->request.context.deadlineUnixMillis <= config.nowUnixMillis()) {
					updateTransfer(job->transferId, [](auto& record) {
						record.status.state = TransferState::Failed;
						record.status.detail = "Transfer deadline elapsed while queued";
						record.error = ServiceError { ServiceErrorCode::DeadlineExceeded, "Transfer deadline elapsed while queued", true };
					});
					continue;
				}
				updateTransfer(job->transferId, [](auto& record) {
					record.status.state = TransferState::Preparing;
					record.status.progress = 0.05;
					record.status.detail = "Preparing edit-buffer messages";
				});
				auto result = backend.sendToEditBuffer(job->request.configuredSynthInstanceId, job->request.patch,
					job->cancelled, [this, id = job->transferId](double progress, std::string detail) {
						updateTransfer(id, [&](auto& record) {
							record.status.state = TransferState::Sending;
							record.status.progress = std::clamp(progress, 0.0, 1.0);
							record.status.detail = std::move(detail);
						});
					});
				updateTransfer(job->transferId, [&](auto& record) {
					if (job->cancelled.load() || (!result && result.error().code == ServiceErrorCode::TransferCancelled)) {
						record.status.state = TransferState::Cancelled;
						record.status.detail = "Transfer cancelled";
						record.error = ServiceError { ServiceErrorCode::TransferCancelled, "Transfer cancelled", false };
					}
					else if (!result) {
						record.status.state = TransferState::Failed;
						record.status.detail = result.error().message;
						record.error = result.error();
					}
					else {
						record.status.state = TransferState::Succeeded;
						record.status.verification = VerificationState::Unverified;
						record.status.progress = 1.0;
						record.status.detail = "Sent to edit buffer (unverified)";
					}
				});
			}
		}

		void enqueue(std::shared_ptr<Job> const& job) {
			DeviceQueue* queue = nullptr;
			{
				std::lock_guard lock(stateMutex);
				auto& item = deviceQueues[job->request.configuredSynthInstanceId];
				if (!item) {
					item = std::make_unique<DeviceQueue>();
					queue = item.get();
					queue->worker = std::thread([this, queue] { process(*queue); });
				}
				else queue = item.get();
			}
			{
				std::lock_guard lock(queue->mutex);
				queue->jobs.push_back(job);
			}
			queue->changed.notify_one();
		}
	};

	SessionServiceAdapter::SessionServiceAdapter(EngineSessionBackend& backend, SessionServiceAdapterConfig config)
		: impl_(std::make_unique<Impl>(backend, std::move(config))) {}
	SessionServiceAdapter::~SessionServiceAdapter() = default;

	ServiceResult<ServiceResponse<ServerInfo>> SessionServiceAdapter::getServerInfo(RequestContext const& request) {
		if (auto error = impl_->validate(request)) return failure<ServerInfo>(*error);
		return response(request.requestId, ServerInfo { "KnobKraft Orm", "Recall bridge", {}, CURRENT_SESSION_PROTOCOL_MAJOR, CURRENT_SESSION_PROTOCOL_MINOR });
	}

	ServiceResult<ServiceResponse<PagedItems<SessionSynthInfo>>> SessionServiceAdapter::listConfiguredSynthInstances(ListSynthsRequest const& request) {
		if (auto error = impl_->validate(request.context)) return failure<PagedItems<SessionSynthInfo>>(*error);
		auto offset = pageOffset(request.page.pageToken);
		if (!offset || request.page.pageSize == 0 || request.page.pageSize > impl_->config.maximumPageSize)
			return failure<PagedItems<SessionSynthInfo>>({ ServiceErrorCode::InvalidRequest, "Invalid synth page", false });
		auto synths = impl_->backend.configuredSynths();
		PagedItems<SessionSynthInfo> page;
		for (std::size_t i = *offset; i < synths.size() && page.items.size() < request.page.pageSize; ++i) page.items.push_back(project(synths[i]));
		if (*offset + page.items.size() < synths.size()) page.nextPageToken = std::to_string(*offset + page.items.size());
		return response(request.context.requestId, std::move(page));
	}

	ServiceResult<ServiceResponse<SessionSynthInfo>> SessionServiceAdapter::getConfiguredSynthInstance(GetSynthRequest const& request) {
		if (auto error = impl_->validate(request.context)) return failure<SessionSynthInfo>(*error);
		auto synth = impl_->findSynth(request.configuredSynthInstanceId);
		if (!synth) return failure<SessionSynthInfo>({ ServiceErrorCode::ConfiguredSynthMissing, "Configured synth was not found", false });
		return response(request.context.requestId, project(*synth));
	}

	ServiceResult<ServiceResponse<PagedItems<PatchSummary>>> SessionServiceAdapter::searchPatches(SearchPatchesRequest const& request) {
		if (auto error = impl_->validate(request.context)) return failure<PagedItems<PatchSummary>>(*error);
		auto offset = pageOffset(request.page.pageToken);
		if (!offset || request.page.pageSize == 0 || request.page.pageSize > impl_->config.maximumPageSize)
			return failure<PagedItems<PatchSummary>>({ ServiceErrorCode::InvalidRequest, "Invalid patch page", false });
		auto source = impl_->backend.searchPatches(request.query, request.adaptationId, *offset, request.page.pageSize);
		PagedItems<PatchSummary> page;
		for (auto const& item : source.patches) {
			auto patch = project(item);
			if (patch.fingerprint.empty()) return failure<PagedItems<PatchSummary>>({ ServiceErrorCode::PatchDataInvalid, "Database patch could not be fingerprinted", false });
			page.items.push_back({ item.patchId, patch.adaptationId, patch.dataTypeId, patch.name, patch.fingerprint, patch.source });
		}
		if (source.hasMore) page.nextPageToken = std::to_string(*offset + page.items.size());
		return response(request.context.requestId, std::move(page));
	}

	ServiceResult<ServiceResponse<SessionPatch>> SessionServiceAdapter::getPatch(GetPatchRequest const& request) {
		if (auto error = impl_->validate(request.context)) return failure<SessionPatch>(*error);
		auto source = impl_->backend.getPatch(request.patchId);
		if (!source) return failure<SessionPatch>({ ServiceErrorCode::PatchNotFound, "Patch was not found", false });
		auto patch = project(*source);
		if (patch.fingerprint.empty()) return failure<SessionPatch>({ ServiceErrorCode::PatchDataInvalid, "Database patch could not be fingerprinted", false });
		return response(request.context.requestId, std::move(patch));
	}

	ServiceResult<ServiceResponse<TransferRecord>> SessionServiceAdapter::applyToEditBuffer(ApplyToEditBufferRequest const& request) {
		if (auto error = impl_->validate(request.context)) return failure<TransferRecord>(*error);
		{
			std::lock_guard lock(impl_->stateMutex);
			auto previous = impl_->requestToTransfer.find(request.context.requestId);
			if (previous != impl_->requestToTransfer.end()) return response(request.context.requestId, impl_->transfers.at(previous->second));
		}
		auto synth = impl_->findSynth(request.configuredSynthInstanceId);
		if (!synth) return failure<TransferRecord>({ ServiceErrorCode::ConfiguredSynthMissing, "Configured synth was not found", false });
		if (request.expectedAdaptationId != synth->adaptationId || request.patch.adaptationId != synth->adaptationId)
			return failure<TransferRecord>({ ServiceErrorCode::PatchIncompatible, "Patch adaptation does not match the configured synth", false });
		if (!synth->capabilities.editBuffer)
			return failure<TransferRecord>({ ServiceErrorCode::AdaptationError, "Configured synth has no edit-buffer capability", false });
		if (!synth->online) return failure<TransferRecord>({ ServiceErrorCode::SynthOffline, "Configured synth is offline", true });
		auto fingerprint = SessionPatchCodec::fingerprint(request.patch);
		if (!fingerprint || fingerprint.value() != request.patch.fingerprint)
			return failure<TransferRecord>({ ServiceErrorCode::PatchDataInvalid, "Patch fingerprint or payload is invalid", false });

		auto job = std::make_shared<Impl::Job>();
		job->request = request;
		job->transferId = impl_->config.transferIdGenerator();
		TransferRecord record;
		record.status = { job->transferId, request.context.requestId, TransferState::Queued, VerificationState::NotAttempted, 0.0, "Queued" };
		record.clientId = request.context.clientId;
		record.pluginInstanceId = request.context.pluginInstanceId;
		record.pluginInstanceName = request.context.pluginInstanceId;
		record.configuredSynthInstanceId = request.configuredSynthInstanceId;
		record.patchName = request.patch.name;
		record.patchFingerprint = request.patch.fingerprint;
		record.updatedAtUnixMillis = impl_->config.nowUnixMillis();
		{
			std::lock_guard lock(impl_->stateMutex);
			auto session = std::find_if(impl_->snapshot.sessions.begin(), impl_->snapshot.sessions.end(), [&](auto const& item) {
				return item.clientId == request.context.clientId && item.pluginInstanceId == request.context.pluginInstanceId;
			});
			if (session != impl_->snapshot.sessions.end() && !session->instanceName.empty()) record.pluginInstanceName = session->instanceName;
			impl_->transfers.emplace(job->transferId, record);
			impl_->requestToTransfer.emplace(request.context.requestId, job->transferId);
			impl_->jobsByTransfer.emplace(job->transferId, job);
		}
		if (impl_->config.auditLog) {
			std::ostringstream text;
			text << "Recall request " << request.context.requestId << " from plugin '" << record.pluginInstanceName << "'"
				 << " queued patch '" << request.patch.name << "' (" << request.patch.fingerprint << ") for " << request.configuredSynthInstanceId;
			impl_->config.auditLog(text.str());
		}
		impl_->emitSnapshot();
		impl_->enqueue(job);
		return response(request.context.requestId, std::move(record));
	}

	ServiceResult<ServiceResponse<TransferRecord>> SessionServiceAdapter::getTransferStatus(GetTransferStatusRequest const& request) {
		if (auto error = impl_->validate(request.context)) return failure<TransferRecord>(*error);
		std::lock_guard lock(impl_->stateMutex);
		auto found = impl_->transfers.find(request.transferId);
		if (found == impl_->transfers.end()) return failure<TransferRecord>({ ServiceErrorCode::TransferNotFound, "Transfer was not found", false });
		return response(request.context.requestId, found->second);
	}

	ServiceResult<ServiceResponse<TransferRecord>> SessionServiceAdapter::cancelTransfer(CancelTransferRequest const& request) {
		if (auto error = impl_->validate(request.context)) return failure<TransferRecord>(*error);
		TransferRecord result;
		{
			std::lock_guard lock(impl_->stateMutex);
			auto transfer = impl_->transfers.find(request.transferId);
			if (transfer == impl_->transfers.end()) return failure<TransferRecord>({ ServiceErrorCode::TransferNotFound, "Transfer was not found", false });
			if (transfer->second.status.state == TransferState::Succeeded || transfer->second.status.state == TransferState::Failed || transfer->second.status.state == TransferState::Cancelled)
				return failure<TransferRecord>({ ServiceErrorCode::CancelNotAllowed, "Transfer has already completed", false });
			impl_->jobsByTransfer.at(request.transferId)->cancelled.store(true);
			transfer->second.status.detail = "Cancellation requested";
			transfer->second.updatedAtUnixMillis = impl_->config.nowUnixMillis();
			result = transfer->second;
		}
		impl_->emitSnapshot();
		return response(request.context.requestId, std::move(result));
	}

	ServiceResult<ServiceResponse<NavigationResult>> SessionServiceAdapter::openKnobKraft(OpenKnobKraftRequest const& request) {
		if (auto error = impl_->validate(request.context)) return failure<NavigationResult>(*error);
		return response(request.context.requestId, NavigationResult { impl_->backend.openKnobKraft(request.target, request.targetId) });
	}

	ServiceResult<ServiceResponse<SessionSnapshot>> SessionServiceAdapter::publishSession(PublishSessionRequest const& request) {
		if (auto error = impl_->validate(request.context)) return failure<SessionSnapshot>(*error);
		{
			std::lock_guard lock(impl_->stateMutex);
			auto found = std::find_if(impl_->snapshot.sessions.begin(), impl_->snapshot.sessions.end(), [&](auto const& session) {
				return session.clientId == request.context.clientId && session.pluginInstanceId == request.context.pluginInstanceId;
			});
			PluginSessionState state { request.context.clientId, request.context.pluginInstanceId, request.instanceName,
				request.hostName, request.binding, request.storedPatchName, request.storedPatchFingerprint, impl_->config.nowUnixMillis() };
			if (found == impl_->snapshot.sessions.end()) impl_->snapshot.sessions.push_back(std::move(state)); else *found = std::move(state);
		}
		impl_->emitSnapshot();
		return response(request.context.requestId, currentSnapshot());
	}

	ServiceResult<ServiceResponse<SessionSnapshot>> SessionServiceAdapter::disconnectSession(DisconnectSessionRequest const& request) {
		if (auto error = impl_->validate(request.context)) return failure<SessionSnapshot>(*error);
		{
			std::lock_guard lock(impl_->stateMutex);
			auto& sessions = impl_->snapshot.sessions;
			sessions.erase(std::remove_if(sessions.begin(), sessions.end(), [&](auto const& session) {
				return session.clientId == request.context.clientId && session.pluginInstanceId == request.context.pluginInstanceId;
			}), sessions.end());
		}
		impl_->emitSnapshot();
		return response(request.context.requestId, currentSnapshot());
	}

	SessionSnapshot SessionServiceAdapter::currentSnapshot() const {
		std::lock_guard lock(impl_->stateMutex);
		return impl_->snapshot;
	}

	ObserverId SessionServiceAdapter::subscribe(SessionObserver observer, bool emitCurrent) {
		ObserverId id;
		SessionSnapshot current;
		{
			std::lock_guard lock(impl_->stateMutex);
			id = impl_->nextObserver++;
			impl_->observers.emplace(id, observer);
			current = impl_->snapshot;
		}
		if (emitCurrent && observer) observer(current);
		return id;
	}

	void SessionServiceAdapter::unsubscribe(ObserverId observerId) {
		std::lock_guard lock(impl_->stateMutex);
		impl_->observers.erase(observerId);
	}
}
