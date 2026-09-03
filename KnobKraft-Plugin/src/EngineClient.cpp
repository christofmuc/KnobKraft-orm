/*
   Copyright (c) 2026 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "EngineClient.h"

#include "SessionCodecs.h"

#include <juce_core/juce_core.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <utility>

namespace knobkraft::recall {
	using Json = nlohmann::json;
	using namespace midikraft::session;

	namespace {
		std::int64_t systemNowUnixMillis() {
			return std::chrono::duration_cast<std::chrono::milliseconds>(
				std::chrono::system_clock::now().time_since_epoch()).count();
		}

		std::string uuid() {
			return juce::Uuid().toString().toStdString();
		}

		std::optional<SessionPatch> selectedPatch(SessionManifest const& manifest) {
			if (!manifest.selectedSoundId) return std::nullopt;
			auto const found = std::find_if(manifest.sounds.begin(), manifest.sounds.end(), [&](auto const& sound) {
				return sound.soundId == *manifest.selectedSoundId;
			});
			if (found == manifest.sounds.end()) return std::nullopt;
			return found->patch;
		}

		ServiceError readError(Json const& value) {
			return { static_cast<ServiceErrorCode>(value.value("code", static_cast<int>(ServiceErrorCode::InternalError))),
				value.value("message", std::string("Unknown KnobKraft error")), value.value("retryable", false) };
		}

		SessionSynthInfo readSynth(Json const& value) {
			SessionSynthInfo result;
			result.configuredSynthInstanceId = value.at("configuredSynthInstanceId").get<std::string>();
			result.displayName = value.at("displayName").get<std::string>();
			result.adaptationId = value.at("adaptationId").get<std::string>();
			result.online = value.value("online", false);
			auto const& capabilities = value.at("capabilities");
			result.capabilities.editBuffer = capabilities.value("editBuffer", false);
			result.capabilities.programDump = capabilities.value("programDump", false);
			result.capabilities.customProgramChange = capabilities.value("customProgramChange", false);
			result.capabilities.verification = capabilities.value("verification", false);
			return result;
		}

		PatchProvenance readProvenance(Json const& value) {
			PatchProvenance result;
			if (value.contains("databaseId")) result.databaseId = value.at("databaseId").get<std::string>();
			if (value.contains("bank")) result.bank = value.at("bank").get<std::int32_t>();
			if (value.contains("program")) result.program = value.at("program").get<std::int32_t>();
			return result;
		}

		PatchSummary readPatchSummary(Json const& value) {
			PatchSummary result;
			result.patchId = value.at("patchId").get<std::string>();
			result.adaptationId = value.at("adaptationId").get<std::string>();
			result.dataTypeId = value.at("dataTypeId").get<std::string>();
			result.name = value.at("name").get<std::string>();
			result.fingerprint = value.at("fingerprint").get<std::string>();
			if (value.contains("source")) result.source = readProvenance(value.at("source"));
			return result;
		}

		TransferRecord readTransfer(Json const& value) {
			TransferRecord result;
			auto const& status = value.at("status");
			result.status.transferId = status.at("transferId").get<std::string>();
			result.status.requestId = status.at("requestId").get<std::string>();
			result.status.state = static_cast<TransferState>(status.at("state").get<int>());
			result.status.verification = static_cast<VerificationState>(status.at("verification").get<int>());
			if (status.contains("progress")) result.status.progress = status.at("progress").get<double>();
			result.status.detail = status.value("detail", std::string {});
			result.clientId = value.value("clientId", std::string {});
			result.pluginInstanceId = value.value("pluginInstanceId", std::string {});
			result.pluginInstanceName = value.value("pluginInstanceName", std::string {});
			result.configuredSynthInstanceId = value.value("configuredSynthInstanceId", std::string {});
			result.patchName = value.value("patchName", std::string {});
			result.patchFingerprint = value.value("patchFingerprint", std::string {});
			result.updatedAtUnixMillis = value.value("updatedAtUnixMillis", std::int64_t {});
			if (value.contains("error")) result.error = readError(value.at("error"));
			return result;
		}

		bool isTerminal(TransferState state) {
			return state == TransferState::Succeeded || state == TransferState::Failed || state == TransferState::Cancelled;
		}
	}

	bool EngineSnapshot::canSend() const noexcept {
		return connection == EngineConnectionState::Connected && binding == BindingState::BoundOnline && !canCancel();
	}

	bool EngineSnapshot::canCancel() const noexcept {
		return transfer && !isTerminal(transfer->status.state);
	}

	std::shared_ptr<EngineSnapshot const> EngineStateReducer::connection(
		std::shared_ptr<EngineSnapshot const> const& current, bool connected) {
		auto next = std::make_shared<EngineSnapshot>(*current);
		++next->revision;
		next->connection = connected ? EngineConnectionState::Connected : EngineConnectionState::Disconnected;
		next->connectionDetail = connected ? "Connected to KnobKraft" : "KnobKraft is not running";
		if (!connected) {
			if (next->binding != BindingState::Unbound) next->binding = BindingState::Resolving;
			next->boundSynth.reset();
		}
		return next;
	}

	std::shared_ptr<EngineSnapshot const> EngineStateReducer::synths(
		std::shared_ptr<EngineSnapshot const> const& current, std::vector<SessionSynthInfo> values,
		SynthBinding const& binding) {
		auto next = std::make_shared<EngineSnapshot>(*current);
		++next->revision;
		next->synths = std::move(values);
		next->error.reset();
		next->boundSynth.reset();
		if (!binding.configuredSynthInstanceId) {
			next->binding = BindingState::Unbound;
			return next;
		}
		auto const found = std::find_if(next->synths.begin(), next->synths.end(), [&](auto const& synth) {
			return synth.configuredSynthInstanceId == *binding.configuredSynthInstanceId;
		});
		if (found == next->synths.end()) {
			next->binding = BindingState::Missing;
			return next;
		}
		next->boundSynth = *found;
		if (binding.fallbackAdaptationId && *binding.fallbackAdaptationId != found->adaptationId)
			next->binding = BindingState::AdaptationMismatch;
		else next->binding = found->online ? BindingState::BoundOnline : BindingState::BoundOffline;
		return next;
	}

	std::shared_ptr<EngineSnapshot const> EngineStateReducer::transfer(
		std::shared_ptr<EngineSnapshot const> const& current, TransferRecord value) {
		auto next = std::make_shared<EngineSnapshot>(*current);
		++next->revision;
		next->transfer = std::move(value);
		next->error = next->transfer->error;
		if (isTerminal(next->transfer->status.state)) next->lastResultUnixMillis = next->transfer->updatedAtUnixMillis;
		return next;
	}

	std::shared_ptr<EngineSnapshot const> EngineStateReducer::failure(
		std::shared_ptr<EngineSnapshot const> const& current, ServiceError value) {
		auto next = std::make_shared<EngineSnapshot>(*current);
		++next->revision;
		next->error = value;
		if (value.code == ServiceErrorCode::ProtocolIncompatible)
			next->connection = EngineConnectionState::ProtocolIncompatible;
		else if (value.code == ServiceErrorCode::AuthenticationFailed)
			next->connection = EngineConnectionState::AuthenticationFailed;
		return next;
	}

	std::filesystem::path defaultDiscoveryFilePath() {
		auto folder = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
			.getChildFile("KnobKraftOrm").getChildFile("recall-session-v1.json");
#if JUCE_WINDOWS
		return std::filesystem::path(folder.getFullPathName().toWideCharPointer());
#else
		return std::filesystem::path(folder.getFullPathName().toStdString());
#endif
	}

	EngineClientSettings defaultEngineClientSettings() {
		EngineClientSettings result;
		result.transport.discoveryFile = std::make_shared<DiscoveryFile>(defaultDiscoveryFilePath());
		result.transport.nowUnixMillis = systemNowUnixMillis;
		result.clientId = uuid();
		result.requestIdGenerator = uuid;
		result.nowUnixMillis = systemNowUnixMillis;
		return result;
	}

	EngineClient::EngineClient(PluginState& state, EngineClientSettings settings)
		: state_(state), settings_(std::move(settings)), client_(settings_.transport),
		  snapshot_(std::make_shared<EngineSnapshot const>()) {
		if (settings_.clientId.empty()) settings_.clientId = uuid();
		if (!settings_.requestIdGenerator) settings_.requestIdGenerator = uuid;
		if (!settings_.nowUnixMillis) settings_.nowUnixMillis = systemNowUnixMillis;
		client_.setConnectionObserver([this](bool connected) { handleConnection(connected); });
		client_.setSnapshotObserver([this](SessionSnapshot const& value) { handleServiceSnapshot(value); });
		worker_ = std::thread([this] { workerLoop(); });
		client_.start();
	}

	EngineClient::~EngineClient() {
		client_.setConnectionObserver({});
		client_.setSnapshotObserver({});
		client_.stop();
		stopping_.store(true);
		tasksChanged_.notify_all();
		if (worker_.joinable()) worker_.join();
	}

	std::shared_ptr<EngineSnapshot const> EngineClient::snapshot() const noexcept {
		return snapshot_.load(std::memory_order_acquire);
	}

	void EngineClient::setSnapshot(std::shared_ptr<EngineSnapshot const> next) {
		snapshot_.store(std::move(next), std::memory_order_release);
		sendChangeMessage();
	}

	void EngineClient::update(std::function<void(EngineSnapshot&)> const& mutation) {
		auto current = snapshot();
		for (;;) {
			auto next = std::make_shared<EngineSnapshot>(*current);
			++next->revision;
			mutation(*next);
			std::shared_ptr<EngineSnapshot const> immutable = next;
			if (snapshot_.compare_exchange_weak(current, immutable, std::memory_order_release, std::memory_order_acquire)) {
				sendChangeMessage();
				return;
			}
		}
	}

	void EngineClient::post(Task task) {
		if (stopping_.load()) return;
		{
			std::lock_guard lock(tasksMutex_);
			tasks_.push_back(std::move(task));
		}
		tasksChanged_.notify_one();
	}

	void EngineClient::workerLoop() {
		for (;;) {
			Task task;
			{
				std::unique_lock lock(tasksMutex_);
				tasksChanged_.wait(lock, [&] { return stopping_.load() || !tasks_.empty(); });
				if (stopping_.load() && tasks_.empty()) return;
				task = std::move(tasks_.front());
				tasks_.pop_front();
			}
			try { task(); }
			catch (std::exception const& error) {
				applyError({ ServiceErrorCode::InternalError, error.what(), false });
			}
		}
	}

	RequestContext EngineClient::context(std::string requestId) const {
		auto const manifest = state_.snapshot()->manifest;
		return { std::move(requestId), settings_.clientId, manifest.pluginInstanceId,
			settings_.nowUnixMillis() + settings_.requestTimeout.count() };
	}

	IpcResponse EngineClient::request(std::string operation, std::string body, std::optional<std::string> stableRequestId) {
		auto requestId = stableRequestId.value_or(settings_.requestIdGenerator());
		return client_.request(std::move(operation), context(requestId), std::move(body)).get();
	}

	void EngineClient::applyError(ServiceError error) {
		setSnapshot(EngineStateReducer::failure(snapshot(), std::move(error)));
	}

	void EngineClient::handleConnection(bool connected) {
		setSnapshot(EngineStateReducer::connection(snapshot(), connected));
		if (connected) {
			refreshSynths();
			publishSession();
		}
	}

	void EngineClient::handleServiceSnapshot(SessionSnapshot const& serviceSnapshot) {
		auto const manifest = state_.snapshot()->manifest;
		std::optional<TransferRecord> latest;
		for (auto const& transfer : serviceSnapshot.transfers) {
			if (transfer.clientId != settings_.clientId || transfer.pluginInstanceId != manifest.pluginInstanceId) continue;
			if (!latest || transfer.updatedAtUnixMillis >= latest->updatedAtUnixMillis) latest = transfer;
		}
		if (latest) setSnapshot(EngineStateReducer::transfer(snapshot(), std::move(*latest)));
	}

	void EngineClient::manifestChanged() {
		if (snapshot()->connection == EngineConnectionState::Connected) {
			refreshSynths();
			publishSession();
		}
	}

	void EngineClient::refreshSynths() {
		post([this] {
			auto response = request(std::string(ipc_operation::LIST_CONFIGURED_SYNTH_INSTANCES), R"({"pageSize":100})");
			if (!response.hasValue()) { applyError(*response.error); return; }
			auto const body = Json::parse(*response.payloadJson);
			std::vector<SessionSynthInfo> values;
			for (auto const& value : body.at("items")) values.push_back(readSynth(value));
			setSnapshot(EngineStateReducer::synths(snapshot(), std::move(values), state_.snapshot()->manifest.binding));
		});
	}

	void EngineClient::resolveBinding() {
		refreshSynths();
	}

	void EngineClient::publishSession() {
		post([this] {
			auto const manifest = state_.snapshot()->manifest;
			auto patch = selectedPatch(manifest);
			Json body { { "instanceName", manifest.instanceName }, { "binding", Json::object() } };
			if (manifest.binding.configuredSynthInstanceId)
				body["binding"]["configuredSynthInstanceId"] = *manifest.binding.configuredSynthInstanceId;
			if (manifest.binding.fallbackAdaptationId)
				body["binding"]["fallbackAdaptationId"] = *manifest.binding.fallbackAdaptationId;
			if (patch) {
				body["storedPatchName"] = patch->name;
				body["storedPatchFingerprint"] = patch->fingerprint;
			}
			auto response = request(std::string(ipc_operation::PUBLISH_SESSION), body.dump());
			if (!response.hasValue()) applyError(*response.error);
		});
	}

	void EngineClient::rebind(std::string configuredSynthInstanceId) {
		auto current = snapshot();
		auto found = std::find_if(current->synths.begin(), current->synths.end(), [&](auto const& synth) {
			return synth.configuredSynthInstanceId == configuredSynthInstanceId;
		});
		if (found == current->synths.end()) {
			applyError({ ServiceErrorCode::ConfiguredSynthMissing, "Select a configured synth before rebinding", false });
			return;
		}
		auto manifest = state_.snapshot()->manifest;
		manifest.binding.configuredSynthInstanceId = found->configuredSynthInstanceId;
		manifest.binding.fallbackAdaptationId = found->adaptationId;
		manifest.recallPolicy = RecallPolicy::Manual;
		if (!state_.replaceManifest(std::move(manifest))) return;
		setSnapshot(EngineStateReducer::synths(snapshot(), current->synths, state_.snapshot()->manifest.binding));
		publishSession();
	}

	void EngineClient::searchPatches(std::string query, bool nextPage) {
		auto const existing = snapshot();
		auto pageToken = nextPage ? existing->nextPatchPageToken : std::optional<std::string> {};
		post([this, query = std::move(query), pageToken, nextPage] {
			Json body { { "query", query }, { "pageSize", 25 } };
			if (pageToken) body["pageToken"] = *pageToken;
			if (auto const adaptation = state_.snapshot()->manifest.binding.fallbackAdaptationId)
				body["adaptationId"] = *adaptation;
			auto response = request(std::string(ipc_operation::SEARCH_PATCHES), body.dump());
			if (!response.hasValue()) { applyError(*response.error); return; }
			auto const value = Json::parse(*response.payloadJson);
			std::vector<PatchSummary> results;
			for (auto const& item : value.at("items")) results.push_back(readPatchSummary(item));
			update([&](EngineSnapshot& state) {
				state.patchQuery = query;
				if (nextPage) state.patchResults.insert(state.patchResults.end(), results.begin(), results.end());
				else state.patchResults = std::move(results);
				state.nextPatchPageToken.reset();
				if (value.contains("nextPageToken")) state.nextPatchPageToken = value.at("nextPageToken").get<std::string>();
				state.error.reset();
			});
		});
	}

	void EngineClient::selectPatch(std::string patchId) {
		post([this, patchId = std::move(patchId)] {
			Json body { { "patchId", patchId } };
			auto response = request(std::string(ipc_operation::GET_PATCH), body.dump());
			if (!response.hasValue()) { applyError(*response.error); return; }
			auto decoded = SessionPatchCodec::decode(*response.payloadJson);
			if (!decoded) {
				applyError({ ServiceErrorCode::PatchDataInvalid, decoded.error().message, false });
				return;
			}
			auto manifest = state_.snapshot()->manifest;
			manifest.recallPolicy = RecallPolicy::Manual;
			manifest.selectedSoundId = patchId;
			manifest.sounds = { SessionSound { patchId, std::move(decoded).value() } };
			if (!state_.replaceManifest(std::move(manifest))) {
				applyError({ ServiceErrorCode::PatchDataInvalid, "KnobKraft returned invalid project state", false });
				return;
			}
			update([](EngineSnapshot& state) { state.error.reset(); });
			publishSession();
		});
	}

	void EngineClient::renameInstance(std::string instanceName) {
		if (instanceName.empty()) return;
		auto manifest = state_.snapshot()->manifest;
		manifest.instanceName = std::move(instanceName);
		manifest.recallPolicy = RecallPolicy::Manual;
		if (state_.replaceManifest(std::move(manifest))) publishSession();
	}

	void EngineClient::sendStoredPatch() {
		auto const stateSnapshot = state_.snapshot();
		auto patch = selectedPatch(stateSnapshot->manifest);
		if (!patch || !stateSnapshot->manifest.binding.configuredSynthInstanceId) {
			applyError({ ServiceErrorCode::InvalidRequest, "Choose a project sound and bind a synth before sending", false });
			return;
		}
		auto encoded = SessionPatchCodec::encode(*patch);
		if (!encoded) {
			applyError({ ServiceErrorCode::PatchDataInvalid, encoded.error().message, false });
			return;
		}
		Json body { { "configuredSynthInstanceId", *stateSnapshot->manifest.binding.configuredSynthInstanceId },
			{ "expectedAdaptationId", patch->adaptationId }, { "patch", Json::parse(encoded.value()) } };
		post([this, body = body.dump()] {
			auto response = request(std::string(ipc_operation::APPLY_TO_EDIT_BUFFER), body);
			if (!response.hasValue()) { applyError(*response.error); return; }
			setSnapshot(EngineStateReducer::transfer(snapshot(), readTransfer(Json::parse(*response.payloadJson))));
		});
	}

	void EngineClient::cancelTransfer() {
		auto current = snapshot();
		if (!current->canCancel()) return;
		auto transferId = current->transfer->status.transferId;
		post([this, transferId = std::move(transferId)] {
			Json body { { "transferId", transferId } };
			auto response = request(std::string(ipc_operation::CANCEL_TRANSFER), body.dump());
			if (!response.hasValue()) { applyError(*response.error); return; }
			setSnapshot(EngineStateReducer::transfer(snapshot(), readTransfer(Json::parse(*response.payloadJson))));
		});
	}

	void EngineClient::openKnobKraft() {
		post([this] {
			Json body { { "target", static_cast<int>(NavigationTargetKind::Application) } };
			auto response = request(std::string(ipc_operation::OPEN_KNOBKRAFT), body.dump());
			if (!response.hasValue()) applyError(*response.error);
		});
	}

}
