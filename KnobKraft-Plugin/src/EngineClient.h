/*
   Copyright (c) 2026 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "PluginState.h"
#include "SessionTransport.h"

#include <juce_events/juce_events.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace knobkraft::recall {

	enum class EngineConnectionState {
		Disconnected,
		Connecting,
		Connected,
		ProtocolIncompatible,
		AuthenticationFailed
	};

	enum class BindingState {
		Unbound,
		Resolving,
		BoundOnline,
		BoundOffline,
		Missing,
		AdaptationMismatch
	};

	struct EngineSnapshot {
		std::uint64_t revision = 0;
		EngineConnectionState connection = EngineConnectionState::Disconnected;
		BindingState binding = BindingState::Unbound;
		std::string connectionDetail = "KnobKraft is not running";
		std::vector<midikraft::session::SessionSynthInfo> synths;
		std::optional<midikraft::session::SessionSynthInfo> boundSynth;
		std::string patchQuery;
		std::vector<midikraft::session::PatchSummary> patchResults;
		std::optional<std::string> nextPatchPageToken;
		std::optional<midikraft::session::TransferRecord> transfer;
		std::optional<midikraft::session::ServiceError> error;
		std::optional<std::int64_t> lastResultUnixMillis;

		[[nodiscard]] bool canSend() const noexcept;
		[[nodiscard]] bool canCancel() const noexcept;
	};

	// Pure reducer used by EngineClient and by UI-state tests. Every method returns
	// a new immutable snapshot, so readers never observe a partly updated state.
	class EngineStateReducer {
	public:
		[[nodiscard]] static std::shared_ptr<EngineSnapshot const> connection(
			std::shared_ptr<EngineSnapshot const> const& current, bool connected);
		[[nodiscard]] static std::shared_ptr<EngineSnapshot const> synths(
			std::shared_ptr<EngineSnapshot const> const& current,
			std::vector<midikraft::session::SessionSynthInfo> values,
			midikraft::session::SynthBinding const& binding);
		[[nodiscard]] static std::shared_ptr<EngineSnapshot const> transfer(
			std::shared_ptr<EngineSnapshot const> const& current,
			midikraft::session::TransferRecord value);
		[[nodiscard]] static std::shared_ptr<EngineSnapshot const> failure(
			std::shared_ptr<EngineSnapshot const> const& current,
			midikraft::session::ServiceError value);
	};

	struct EngineClientSettings {
		midikraft::session::SessionIpcClientConfig transport;
		std::string clientId;
		std::function<std::string()> requestIdGenerator;
		std::function<std::int64_t()> nowUnixMillis;
		std::chrono::milliseconds requestTimeout { 5'000 };
	};

	[[nodiscard]] EngineClientSettings defaultEngineClientSettings();
	[[nodiscard]] std::filesystem::path defaultDiscoveryFilePath();

	class EngineClient final : public juce::ChangeBroadcaster {
	public:
		EngineClient(PluginState& state, EngineClientSettings settings);
		~EngineClient() override;

		EngineClient(EngineClient const&) = delete;
		EngineClient& operator=(EngineClient const&) = delete;

		[[nodiscard]] std::shared_ptr<EngineSnapshot const> snapshot() const noexcept;
		void manifestChanged();
		void refreshSynths();
		void rebind(std::string configuredSynthInstanceId);
		void searchPatches(std::string query, bool nextPage = false);
		void selectPatch(std::string patchId);
		void renameInstance(std::string instanceName);
		void sendStoredPatch();
		void cancelTransfer();
		void openKnobKraft();

	private:
		using Task = std::function<void()>;

		void post(Task task);
		void workerLoop();
		void setSnapshot(std::shared_ptr<EngineSnapshot const> next);
		void update(std::function<void(EngineSnapshot&)> const& mutation);
		void handleConnection(bool connected);
		void handleServiceSnapshot(midikraft::session::SessionSnapshot const& serviceSnapshot);
		void publishSession();
		void resolveBinding();
		[[nodiscard]] midikraft::session::RequestContext context(std::string requestId) const;
		[[nodiscard]] midikraft::session::IpcResponse request(std::string operation, std::string body,
			std::optional<std::string> stableRequestId = std::nullopt);
		void applyError(midikraft::session::ServiceError error);

		PluginState& state_;
		EngineClientSettings settings_;
		midikraft::session::SessionIpcClient client_;
		std::atomic<std::shared_ptr<EngineSnapshot const>> snapshot_;
		std::atomic<bool> stopping_ { false };
		std::mutex tasksMutex_;
		std::condition_variable tasksChanged_;
		std::deque<Task> tasks_;
		std::thread worker_;
	};

}
