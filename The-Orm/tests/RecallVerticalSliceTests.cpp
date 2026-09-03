#include "PluginBridgeServer.h"
#include "PluginProcessor.h"
#include "PluginSessionsModel.h"
#include "SessionServiceAdapter.h"

#include <juce_events/juce_events.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <thread>

using namespace std::chrono_literals;
using namespace midikraft::session;
using namespace knobkraft::recall;
using namespace knobkraft::sessions;

namespace {
	int failures = 0;

	void check(bool condition, char const* expression, int line) {
		if (!condition) {
			std::cerr << "line " << line << ": check failed: " << expression << '\n';
			++failures;
		}
	}

#define CHECK(expression) check(static_cast<bool>(expression), #expression, __LINE__)

	template<typename Predicate>
	bool waitUntil(Predicate predicate, std::chrono::milliseconds timeout = 5s) {
		auto const end = std::chrono::steady_clock::now() + timeout;
		while (std::chrono::steady_clock::now() < end) {
			if (predicate()) return true;
			std::this_thread::sleep_for(10ms);
		}
		return predicate();
	}

	std::int64_t nowUnixMillis() {
		return std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::system_clock::now().time_since_epoch()).count();
	}

	class TestLease final : public PrimaryServerLease {
	public:
		bool tryAcquire() override { acquired_ = true; return true; }
		void release() override { acquired_ = false; }
	private:
		bool acquired_ = false;
	};

	class MatrixBackend final : public EngineSessionBackend {
	public:
		MatrixBackend() {
			ConfiguredSynthInstance synth;
			synth.instanceId = "matrix-main";
			synth.displayName = "Studio Matrix-1000";
			synth.adaptationId = "Oberheim Matrix 1000";
			synth.online = true;
			synth.capabilities.editBuffer = true;
			synths_.push_back(std::move(synth));

			patch_.patchId = "matrix:warm-bass";
			patch_.adaptationId = "Oberheim Matrix 1000";
			patch_.dataTypeId = "0";
			patch_.name = "Warm Bass";
			patch_.payload = { 0xf0, 0x10, 0x06, 0x0d, 0x01, 0x02, 0xf7 };
			patch_.source = PatchProvenance { "matrix-warm-bass", 0, 7 };
		}

		std::vector<ConfiguredSynthInstance> configuredSynths() override { return synths_; }

		PatchSearchPage searchPatches(std::string const& query, std::optional<std::string> const& adaptationId,
			std::size_t offset, std::size_t limit) override {
			PatchSearchPage result;
			if (offset == 0 && limit > 0 && (!adaptationId || *adaptationId == patch_.adaptationId)
				&& (query.empty() || patch_.name.find(query) != std::string::npos)) result.patches.push_back(patch_);
			return result;
		}

		std::optional<EnginePatch> getPatch(std::string const& patchId) override {
			return patchId == patch_.patchId ? std::optional<EnginePatch>(patch_) : std::nullopt;
		}

		ServiceResult<bool> sendToEditBuffer(std::string const& configuredSynthInstanceId,
			SessionPatch const& patch, std::atomic_bool const& cancelled, Progress progress) override {
			CHECK(configuredSynthInstanceId == "matrix-main");
			CHECK(patch.name == "Warm Bass");
			if (cancelled.load()) return ServiceResult<bool>::failure(
				{ ServiceErrorCode::TransferCancelled, "cancelled", false });
			progress(0.4, "Converted Matrix patch");
			std::this_thread::sleep_for(20ms);
			if (cancelled.load()) return ServiceResult<bool>::failure(
				{ ServiceErrorCode::TransferCancelled, "cancelled", false });
			progress(1.0, "Sent Matrix edit buffer");
			++sendCount;
			return ServiceResult<bool>::success(true);
		}

		bool openKnobKraft(NavigationTargetKind, std::optional<std::string> const&) override { return true; }

		std::atomic_int sendCount { 0 };

	private:
		std::vector<ConfiguredSynthInstance> synths_;
		EnginePatch patch_;
	};

	void completeMatrixVerticalSlice() {
		auto directory = std::filesystem::temp_directory_path()
			/ ("knobkraft-wp08-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
		std::filesystem::create_directories(directory);
		auto discovery = std::make_shared<DiscoveryFile>(directory / "recall-session-v1.json");
		MatrixBackend backend;
		SessionServiceAdapter service(backend);
		PluginBridgeServer bridge(service, discovery, std::make_unique<TestLease>());
		CHECK(bridge.start());

		std::atomic_uint64_t requestCounter { 0 };
		EngineClientSettings settings;
		settings.transport.discoveryFile = discovery;
		settings.transport.maximumDiscoveryAge = 10s;
		settings.transport.reconnectDelay = 20ms;
		settings.transport.heartbeatInterval = 50ms;
		settings.clientId = "wp08-ableton";
		settings.requestIdGenerator = [&] { return "wp08-request-" + std::to_string(++requestCounter); };
		settings.nowUnixMillis = nowUnixMillis;

		PluginSessionsController sessions(nowUnixMillis,
			[&] { return "wp08-widget-" + std::to_string(++requestCounter); });
		auto synths = service.listConfiguredSynthInstances(
			{ { "wp08-widget-synths", "knobkraft-standalone", {}, std::nullopt }, { 50, std::nullopt } });
		CHECK(synths);
		if (synths) sessions.setSynths(synths.value().value.items);
		sessions.setService(&service);

		{
			PluginProcessor processor(std::move(settings));
			auto& client = processor.engineClient();
			CHECK(waitUntil([&] { return client.snapshot()->connection == EngineConnectionState::Connected; }));
			CHECK(waitUntil([&] { return client.snapshot()->synths.size() == 1; }));
			client.rebind("matrix-main");
			CHECK(waitUntil([&] { return client.snapshot()->binding == BindingState::BoundOnline; }));
			client.searchPatches("Warm");
			CHECK(waitUntil([&] { return client.snapshot()->patchResults.size() == 1; }));
			client.selectPatch("matrix:warm-bass");
			CHECK(waitUntil([&] {
				auto const state = processor.state().snapshot();
				return !state->manifest.sounds.empty() && state->manifest.sounds.front().patch.name == "Warm Bass";
			}));
			CHECK(waitUntil([&] { return sessions.view().rows.size() == 1; }));
			client.sendStoredPatch();
			CHECK(waitUntil([&] {
				auto const snapshot = client.snapshot();
				return snapshot->transfer && snapshot->transfer->status.state == TransferState::Succeeded;
			}));
			CHECK(backend.sendCount.load() == 1);
			CHECK(waitUntil([&] {
				auto const view = sessions.view();
				return view.rows.size() == 1 && view.rows.front().lastResult
					&& view.rows.front().lastResult->state == TransferState::Succeeded;
			}));
		}

		CHECK(waitUntil([&] { return sessions.view().rows.empty(); }));
		sessions.setService(nullptr);
		bridge.stop();
		std::error_code ignored;
		std::filesystem::remove_all(directory, ignored);
	}
}

int main() {
	juce::ScopedJuceInitialiser_GUI initialiseJuce;
	completeMatrixVerticalSlice();
	if (failures != 0) std::cerr << failures << " Recall vertical-slice test(s) failed\n";
	return failures == 0 ? 0 : 1;
}
