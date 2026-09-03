/*
   Copyright (c) 2026 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "SessionService.h"

#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace knobkraft::sessions {

	enum class PillMode {
		Disconnected,
		Connected,
		ActiveTransfer,
		Attention
	};

	enum class SynthState {
		Unbound,
		Missing,
		Offline,
		Online
	};

	enum class NavigationTarget {
		ResolveBinding,
		Synth,
		Patch
	};

	struct NavigationIntent {
		NavigationTarget target = NavigationTarget::ResolveBinding;
		std::string pluginInstanceId;
		std::optional<std::string> targetId;

		bool operator==(NavigationIntent const& other) const = default;
	};

	struct OperationView {
		std::string transferId;
		midikraft::session::TransferState state = midikraft::session::TransferState::Accepted;
		std::string stateText;
		std::string detail;
		std::optional<double> progress;
		std::int64_t updatedAtUnixMillis = 0;
		bool cancellable = false;
		bool failed = false;

		bool operator==(OperationView const& other) const = default;
	};

	struct SessionRow {
		std::string clientId;
		std::string pluginInstanceId;
		std::string instanceName;
		std::optional<std::string> hostName;
		std::optional<std::string> configuredSynthInstanceId;
		std::string synthName;
		SynthState synthState = SynthState::Unbound;
		std::optional<std::string> patchName;
		std::optional<std::string> patchFingerprint;
		std::optional<OperationView> activeOperation;
		std::optional<OperationView> lastResult;
		bool attention = false;
		std::string attentionMessage;
		std::int64_t lastSeenUnixMillis = 0;

		bool operator==(SessionRow const& other) const = default;
	};

	struct SessionsView {
		PillMode pillMode = PillMode::Disconnected;
		std::string pillText = "No plugins connected";
		std::vector<SessionRow> rows;

		bool operator==(SessionsView const& other) const = default;
	};

	// Pure reducer. It retains only acknowledged/result history; all live data is
	// rebuilt from immutable SessionService snapshots and synth projections.
	class PluginSessionsModel {
	public:
		explicit PluginSessionsModel(std::chrono::milliseconds staleTimeout = std::chrono::seconds(3));

		void reduce(midikraft::session::SessionSnapshot const& snapshot,
			std::vector<midikraft::session::SessionSynthInfo> const& synths,
			std::int64_t nowUnixMillis);
		void acknowledgeAttention(std::string const& pluginInstanceId);
		[[nodiscard]] SessionsView const& view() const noexcept;

	private:
		struct ResultHistory {
			std::optional<OperationView> result;
			bool errorAcknowledged = false;
		};

		std::chrono::milliseconds staleTimeout_;
		SessionsView view_;
		std::map<std::string, ResultHistory> history_;
		midikraft::session::SessionSnapshot lastSnapshot_;
		std::vector<midikraft::session::SessionSynthInfo> lastSynths_;
		std::int64_t lastNowUnixMillis_ = 0;
	};

	struct ActionResult {
		bool accepted = false;
		std::string message;
	};

	// UI-neutral coordinator. A JUCE component may listen to it, but service
	// callbacks, cancellation, expiry and navigation do not know about JUCE.
	class PluginSessionsController {
	public:
		using Clock = std::function<std::int64_t()>;
		using RequestIdFactory = std::function<std::string()>;
		using ViewChanged = std::function<void()>;
		using NavigationHandler = std::function<void(NavigationIntent const&)>;

		explicit PluginSessionsController(Clock clock = {}, RequestIdFactory requestIds = {},
			std::chrono::milliseconds staleTimeout = std::chrono::seconds(3));
		~PluginSessionsController();

		PluginSessionsController(PluginSessionsController const&) = delete;
		PluginSessionsController& operator=(PluginSessionsController const&) = delete;

		// The attached service must outlive the controller, or be detached before
		// it is destroyed. This mirrors the application's explicit server shutdown.
		void setService(midikraft::session::SessionService* service);
		void setSynths(std::vector<midikraft::session::SessionSynthInfo> synths);
		void tick();
		void acknowledgeAttention(std::string const& pluginInstanceId);
		[[nodiscard]] SessionsView view() const;

		void setViewChanged(ViewChanged callback);
		void setNavigationHandler(NavigationHandler callback);
		[[nodiscard]] ActionResult cancel(std::string const& pluginInstanceId, std::string const& transferId);
		[[nodiscard]] ActionResult navigate(std::string const& pluginInstanceId, NavigationTarget target);

	private:
		void receiveSnapshot(midikraft::session::SessionSnapshot const& snapshot);
		void rebuildLocked();
		void notify(ViewChanged callback) const;

		mutable std::mutex mutex_;
		PluginSessionsModel model_;
		midikraft::session::SessionService* service_ = nullptr;
		midikraft::session::ObserverId observerId_ = 0;
		midikraft::session::SessionSnapshot snapshot_;
		std::vector<midikraft::session::SessionSynthInfo> synths_;
		Clock clock_;
		RequestIdFactory requestIds_;
		ViewChanged viewChanged_;
		NavigationHandler navigationHandler_;
	};

}
