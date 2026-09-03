/*
   Copyright (c) 2026 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "PluginSessionsModel.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <sstream>
#include <utility>

namespace knobkraft::sessions {

	namespace {
		using namespace midikraft::session;

		bool isTerminal(TransferState state) {
			return state == TransferState::Succeeded || state == TransferState::Failed || state == TransferState::Cancelled;
		}

		std::string stateText(TransferState state) {
			switch (state) {
			case TransferState::Accepted: return "Accepted";
			case TransferState::Queued: return "Queued";
			case TransferState::Preparing: return "Preparing";
			case TransferState::Sending: return "Sending";
			case TransferState::Verifying: return "Verifying";
			case TransferState::Succeeded: return "Sent";
			case TransferState::Failed: return "Failed";
			case TransferState::Cancelled: return "Cancelled";
			}
			return "Unknown";
		}

		OperationView operationView(TransferRecord const& transfer) {
			return { transfer.status.transferId, transfer.status.state, stateText(transfer.status.state),
				transfer.status.detail, transfer.status.progress, transfer.updatedAtUnixMillis,
				!isTerminal(transfer.status.state), transfer.status.state == TransferState::Failed };
		}

		std::string historyKey(std::string const& clientId, std::string const& pluginInstanceId) {
			return clientId + "\n" + pluginInstanceId;
		}

		std::int64_t systemNowMillis() {
			return std::chrono::duration_cast<std::chrono::milliseconds>(
				std::chrono::system_clock::now().time_since_epoch()).count();
		}

		std::string defaultRequestId() {
			static std::atomic<std::uint64_t> sequence { 1 };
			std::ostringstream stream;
			stream << "knobkraft-widget-" << systemNowMillis() << '-' << sequence.fetch_add(1);
			return stream.str();
		}
	}

	PluginSessionsModel::PluginSessionsModel(std::chrono::milliseconds staleTimeout)
		: staleTimeout_(staleTimeout) {}

	void PluginSessionsModel::reduce(SessionSnapshot const& snapshot, std::vector<SessionSynthInfo> const& synths,
		std::int64_t nowUnixMillis) {
		lastSnapshot_ = snapshot;
		lastSynths_ = synths;
		lastNowUnixMillis_ = nowUnixMillis;

		for (auto const& transfer : snapshot.transfers) {
			if (!isTerminal(transfer.status.state)) continue;
			auto const key = historyKey(transfer.clientId, transfer.pluginInstanceId);
			auto const candidate = operationView(transfer);
			auto& history = history_[key];
			if (!history.result || candidate.updatedAtUnixMillis >= history.result->updatedAtUnixMillis) {
				if (!history.result || candidate.transferId != history.result->transferId
					|| candidate.state != history.result->state) history.errorAcknowledged = false;
				history.result = candidate;
			}
		}

		SessionsView next;
		for (auto const& session : snapshot.sessions) {
			if (nowUnixMillis - session.lastSeenUnixMillis > staleTimeout_.count()) continue;

			SessionRow row;
			row.clientId = session.clientId;
			row.pluginInstanceId = session.pluginInstanceId;
			row.instanceName = session.instanceName;
			row.hostName = session.hostName;
			row.configuredSynthInstanceId = session.binding.configuredSynthInstanceId;
			row.patchName = session.storedPatchName;
			row.patchFingerprint = session.storedPatchFingerprint;
			row.lastSeenUnixMillis = session.lastSeenUnixMillis;

			if (!session.binding.configuredSynthInstanceId) {
				row.synthName = "Not bound";
				row.synthState = SynthState::Unbound;
				row.attention = true;
				row.attentionMessage = "Choose a configured synth";
			}
			else {
				auto const found = std::find_if(synths.begin(), synths.end(), [&](auto const& synth) {
					return synth.configuredSynthInstanceId == *session.binding.configuredSynthInstanceId;
				});
				if (found == synths.end()) {
					row.synthName = *session.binding.configuredSynthInstanceId;
					row.synthState = SynthState::Missing;
					row.attention = true;
					row.attentionMessage = "Configured synth is missing";
				}
				else {
					row.synthName = found->displayName;
					row.synthState = found->online ? SynthState::Online : SynthState::Offline;
					if (!found->online) {
						row.attention = true;
						row.attentionMessage = "Synth is offline";
					}
				}
			}

			for (auto const& transfer : snapshot.transfers) {
				if (transfer.clientId != session.clientId || transfer.pluginInstanceId != session.pluginInstanceId
					|| isTerminal(transfer.status.state)) continue;
				auto candidate = operationView(transfer);
				if (!row.activeOperation || candidate.updatedAtUnixMillis >= row.activeOperation->updatedAtUnixMillis)
					row.activeOperation = std::move(candidate);
			}

			auto const history = history_.find(historyKey(session.clientId, session.pluginInstanceId));
			if (history != history_.end()) {
				row.lastResult = history->second.result;
				if (row.lastResult && row.lastResult->failed && !history->second.errorAcknowledged) {
					row.attention = true;
					row.attentionMessage = row.lastResult->detail.empty() ? "Transfer failed" : row.lastResult->detail;
				}
			}
			next.rows.push_back(std::move(row));
		}

		std::sort(next.rows.begin(), next.rows.end(), [](auto const& left, auto const& right) {
			if (left.instanceName != right.instanceName) return left.instanceName < right.instanceName;
			return left.pluginInstanceId < right.pluginInstanceId;
		});

		auto const attention = std::find_if(next.rows.begin(), next.rows.end(), [](auto const& row) { return row.attention; });
		auto const active = std::find_if(next.rows.begin(), next.rows.end(), [](auto const& row) { return row.activeOperation.has_value(); });
		if (next.rows.empty()) {
			next.pillMode = PillMode::Disconnected;
			next.pillText = "No plugins connected";
		}
		else if (attention != next.rows.end()) {
			next.pillMode = PillMode::Attention;
			next.pillText = "Plugin needs attention";
		}
		else if (active != next.rows.end()) {
			next.pillMode = PillMode::ActiveTransfer;
			next.pillText = active->activeOperation->stateText + " " + active->patchName.value_or("patch")
				+ " to " + active->synthName;
		}
		else {
			next.pillMode = PillMode::Connected;
			next.pillText = std::to_string(next.rows.size()) + (next.rows.size() == 1 ? " plugin session" : " plugin sessions");
		}
		view_ = std::move(next);
	}

	void PluginSessionsModel::acknowledgeAttention(std::string const& pluginInstanceId) {
		for (auto& [key, history] : history_) {
			(void) key;
			if (history.result && history.result->failed) {
				auto const row = std::find_if(view_.rows.begin(), view_.rows.end(), [&](auto const& item) {
					return item.pluginInstanceId == pluginInstanceId && item.lastResult
						&& item.lastResult->transferId == history.result->transferId;
				});
				if (row != view_.rows.end()) history.errorAcknowledged = true;
			}
		}
		reduce(lastSnapshot_, lastSynths_, lastNowUnixMillis_);
	}

	SessionsView const& PluginSessionsModel::view() const noexcept { return view_; }

	PluginSessionsController::PluginSessionsController(Clock clock, RequestIdFactory requestIds,
		std::chrono::milliseconds staleTimeout)
		: model_(staleTimeout), clock_(clock ? std::move(clock) : Clock(systemNowMillis)),
		requestIds_(requestIds ? std::move(requestIds) : RequestIdFactory(defaultRequestId)) {}

	PluginSessionsController::~PluginSessionsController() { setService(nullptr); }

	void PluginSessionsController::setService(SessionService* service) {
		SessionService* oldService = nullptr;
		ObserverId oldObserver = 0;
		{
			std::lock_guard lock(mutex_);
			if (service_ == service) return;
			oldService = service_;
			oldObserver = observerId_;
			service_ = service;
			observerId_ = 0;
			snapshot_ = {};
			rebuildLocked();
		}
		if (oldService && oldObserver != 0) oldService->unsubscribe(oldObserver);
		if (service) {
			auto const observer = service->subscribe([this](SessionSnapshot const& snapshot) { receiveSnapshot(snapshot); });
			std::lock_guard lock(mutex_);
			if (service_ == service) observerId_ = observer;
			else service->unsubscribe(observer);
		}
		ViewChanged callback;
		{
			std::lock_guard lock(mutex_);
			callback = viewChanged_;
		}
		notify(std::move(callback));
	}

	void PluginSessionsController::setSynths(std::vector<SessionSynthInfo> synths) {
		ViewChanged callback;
		{
			std::lock_guard lock(mutex_);
			synths_ = std::move(synths);
			rebuildLocked();
			callback = viewChanged_;
		}
		notify(std::move(callback));
	}

	void PluginSessionsController::tick() {
		ViewChanged callback;
		{
			std::lock_guard lock(mutex_);
			rebuildLocked();
			callback = viewChanged_;
		}
		notify(std::move(callback));
	}

	void PluginSessionsController::acknowledgeAttention(std::string const& pluginInstanceId) {
		ViewChanged callback;
		{
			std::lock_guard lock(mutex_);
			model_.acknowledgeAttention(pluginInstanceId);
			callback = viewChanged_;
		}
		notify(std::move(callback));
	}

	SessionsView PluginSessionsController::view() const {
		std::lock_guard lock(mutex_);
		return model_.view();
	}

	void PluginSessionsController::setViewChanged(ViewChanged callback) {
		std::lock_guard lock(mutex_);
		viewChanged_ = std::move(callback);
	}

	void PluginSessionsController::setNavigationHandler(NavigationHandler callback) {
		std::lock_guard lock(mutex_);
		navigationHandler_ = std::move(callback);
	}

	ActionResult PluginSessionsController::cancel(std::string const& pluginInstanceId, std::string const& transferId) {
		SessionService* service = nullptr;
		SessionRow row;
		{
			std::lock_guard lock(mutex_);
			service = service_;
			auto const found = std::find_if(model_.view().rows.begin(), model_.view().rows.end(), [&](auto const& item) {
				return item.pluginInstanceId == pluginInstanceId && item.activeOperation
					&& item.activeOperation->transferId == transferId;
			});
			if (found == model_.view().rows.end()) return { false, "Active transfer was not found" };
			row = *found;
		}
		if (!service) return { false, "Session service is not connected" };
		auto const now = clock_();
		CancelTransferRequest request { { requestIds_(), "knobkraft-session-widget", row.pluginInstanceId, now + 5000 }, transferId };
		auto result = service->cancelTransfer(request);
		return result ? ActionResult { true, result.value().value.status.detail }
			: ActionResult { false, result.error().message };
	}

	ActionResult PluginSessionsController::navigate(std::string const& pluginInstanceId, NavigationTarget target) {
		NavigationHandler handler;
		NavigationIntent intent;
		{
			std::lock_guard lock(mutex_);
			auto const found = std::find_if(model_.view().rows.begin(), model_.view().rows.end(), [&](auto const& row) {
				return row.pluginInstanceId == pluginInstanceId;
			});
			if (found == model_.view().rows.end()) return { false, "Plugin session was not found" };
			intent.target = target;
			intent.pluginInstanceId = pluginInstanceId;
			if (target == NavigationTarget::Synth || target == NavigationTarget::ResolveBinding)
				intent.targetId = found->configuredSynthInstanceId;
			else if (target == NavigationTarget::Patch)
				intent.targetId = found->patchFingerprint;
			handler = navigationHandler_;
		}
		if (!handler) return { false, "Navigation is not available" };
		handler(intent);
		return { true, "Navigation requested" };
	}

	void PluginSessionsController::receiveSnapshot(SessionSnapshot const& snapshot) {
		ViewChanged callback;
		{
			std::lock_guard lock(mutex_);
			snapshot_ = snapshot;
			rebuildLocked();
			callback = viewChanged_;
		}
		notify(std::move(callback));
	}

	void PluginSessionsController::rebuildLocked() { model_.reduce(snapshot_, synths_, clock_()); }

	void PluginSessionsController::notify(ViewChanged callback) const {
		if (callback) callback();
	}

}
