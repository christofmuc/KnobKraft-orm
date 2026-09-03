/*
   Copyright (c) 2026 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "EngineClient.h"
#include "FakeSessionService.h"
#include "PluginProcessor.h"
#include "PluginState.h"
#include "SessionTransport.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <span>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {
	using namespace std::chrono_literals;
	int failures = 0;
	std::atomic<std::uint64_t> nextTestId { 1 };

	void expect(bool condition, char const* description) {
		if (!condition) {
			std::cerr << "FAILED: " << description << '\n';
			++failures;
		}
	}

	std::filesystem::path testDiscoveryPath(char const* label) {
		return std::filesystem::temp_directory_path() /
			("knobkraft-recall-wp06-" + std::string(label) + "-" + std::to_string(nextTestId.fetch_add(1))) /
			"recall-session-v1.json";
	}

	knobkraft::recall::EngineClientSettings testSettings(std::filesystem::path discovery,
		std::string clientId = "test-client") {
		knobkraft::recall::EngineClientSettings result;
		result.transport.discoveryFile = std::make_shared<midikraft::session::DiscoveryFile>(std::move(discovery));
		result.transport.reconnectDelay = 20ms;
		result.transport.heartbeatInterval = 50ms;
		result.clientId = std::move(clientId);
		result.requestIdGenerator = [] { return "test-request-" + std::to_string(nextTestId.fetch_add(1)); };
		result.nowUnixMillis = [] {
			return std::chrono::duration_cast<std::chrono::milliseconds>(
				std::chrono::system_clock::now().time_since_epoch()).count();
		};
		result.transport.nowUnixMillis = result.nowUnixMillis;
		result.requestTimeout = 2s;
		return result;
	}

	template<typename Predicate>
	bool waitUntil(Predicate predicate, std::chrono::milliseconds timeout = 3s) {
		auto const deadline = std::chrono::steady_clock::now() + timeout;
		while (std::chrono::steady_clock::now() < deadline) {
			if (predicate()) return true;
			std::this_thread::sleep_for(10ms);
		}
		return predicate();
	}

	template<typename Sample>
	void testPassThrough(juce::AudioChannelSet layout, char const* description) {
		knobkraft::recall::PluginProcessor processor(testSettings(testDiscoveryPath(description)));
		juce::AudioProcessor::BusesLayout buses;
		buses.inputBuses.add(layout);
		buses.outputBuses.add(layout);
		expect(processor.setBusesLayout(buses), "processor accepts representative matching layout");
		processor.prepareToPlay(48000.0, 257);

		juce::AudioBuffer<Sample> audio(layout.size(), 257);
		for (int channel = 0; channel < audio.getNumChannels(); ++channel) {
			for (int sample = 0; sample < audio.getNumSamples(); ++sample) {
				audio.setSample(channel, sample, static_cast<Sample>(std::sin(sample * 0.071 + channel)));
			}
		}
		auto const before = audio;
		juce::MidiBuffer midi;
		processor.processBlock(audio, midi);
		bool identical = true;
		for (int channel = 0; channel < audio.getNumChannels(); ++channel) {
			identical = identical && std::memcmp(audio.getReadPointer(channel), before.getReadPointer(channel),
				static_cast<std::size_t>(audio.getNumSamples()) * sizeof(Sample)) == 0;
		}
		expect(identical, description);
		expect(processor.getLatencySamples() == 0, "processor reports zero latency");
	}

	void testStateRoundTrip() {
		using knobkraft::recall::PluginState;
		PluginState source(PluginState::embeddedFixture("source-instance"));
		auto const encoded = source.serialize();
		PluginState restored(PluginState::embeddedFixture("other-instance"));
		expect(restored.restore(encoded), "valid state restores");
		expect(restored.snapshot()->manifest == source.snapshot()->manifest, "manifest survives state round trip");
		expect(restored.serialize() == encoded, "valid state bytes survive state round trip");
	}

	void testCorruptStatePreservation() {
		using knobkraft::recall::PluginState;
		PluginState state(PluginState::embeddedFixture("recoverable-instance"));
		auto const validManifest = state.snapshot()->manifest;
		std::string const corrupt = "{ not a manifest }";
		auto const raw = std::span(reinterpret_cast<std::uint8_t const*>(corrupt.data()), corrupt.size());
		expect(!state.restore(raw), "corrupt state is rejected");
		expect(state.snapshot()->manifest == validManifest, "corrupt state retains last valid manifest for display");
		expect(state.snapshot()->hasDecodeError(), "corrupt state exposes an error");
		expect(state.snapshot()->decodeError->rawStatePreserved, "bounded corrupt state is marked preserved");
		expect(state.serialize() == std::vector<std::uint8_t>(raw.begin(), raw.end()), "corrupt state is saved back verbatim");
	}

	void testFutureStatePreservation() {
		using knobkraft::recall::PluginState;
		PluginState state(PluginState::embeddedFixture("future-instance"));
		auto encoded = state.serialize();
		auto text = std::string(encoded.begin(), encoded.end());
		auto const version = text.find("\"schemaVersion\":1");
		expect(version != std::string::npos, "fixture contains schema version");
		if (version == std::string::npos) return;
		text.replace(version, std::string("\"schemaVersion\":1").size(), "\"schemaVersion\":2");
		auto const raw = std::span(reinterpret_cast<std::uint8_t const*>(text.data()), text.size());
		expect(!state.restore(raw), "future-version state is rejected");
		expect(state.snapshot()->hasDecodeError(), "future-version state exposes an error");
		expect(state.snapshot()->decodeError->code == midikraft::session::CodecErrorCode::UnsupportedVersion,
			"future-version state reports unsupported version");
		expect(state.serialize() == std::vector<std::uint8_t>(raw.begin(), raw.end()), "future-version state is saved back verbatim");
	}

	void testProcessorStateDuplication() {
		knobkraft::recall::PluginProcessor source(testSettings(testDiscoveryPath("duplicate-source"), "duplicate-source"));
		juce::MemoryBlock state;
		source.getStateInformation(state);
		knobkraft::recall::PluginProcessor duplicate(testSettings(testDiscoveryPath("duplicate-copy"), "duplicate-copy"));
		duplicate.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
		expect(duplicate.state().snapshot()->manifest == source.state().snapshot()->manifest, "processor duplication restores embedded state");
	}

	void testProcessorPersistsManualPolicyOnly() {
		using midikraft::session::RecallPolicy;
		using knobkraft::recall::PluginState;
		PluginState futurePolicy(PluginState::embeddedFixture("policy-source"));
		auto manifest = futurePolicy.snapshot()->manifest;
		manifest.recallPolicy = RecallPolicy::AutomaticWhenStopped;
		expect(futurePolicy.replaceManifest(std::move(manifest)), "test fixture accepts a future recall policy");
		auto const bytes = futurePolicy.serialize();
		knobkraft::recall::PluginProcessor processor(testSettings(testDiscoveryPath("manual-policy"), "manual-policy"));
		processor.setStateInformation(bytes.data(), static_cast<int>(bytes.size()));
		expect(processor.state().snapshot()->manifest.recallPolicy == RecallPolicy::Manual,
			"WP-06 normalizes restored policy to manual");
	}

	void testInvalidEditDoesNotEraseState() {
		using knobkraft::recall::PluginState;
		PluginState state(PluginState::embeddedFixture("edit-instance"));
		auto const validManifest = state.snapshot()->manifest;
		auto const validBytes = state.serialize();
		auto invalidManifest = validManifest;
		invalidManifest.selectedSoundId = "missing-sound";
		expect(!state.replaceManifest(std::move(invalidManifest)), "invalid manifest edit is rejected");
		expect(state.snapshot()->manifest == validManifest, "invalid manifest edit retains the valid manifest");
		expect(state.serialize() == validBytes, "invalid manifest edit retains serialized project state");
		expect(state.snapshot()->hasDecodeError(), "invalid manifest edit exposes an error");
	}

	void testEngineStateModel() {
		using namespace knobkraft::recall;
		using namespace midikraft::session;
		auto state = std::make_shared<EngineSnapshot const>();
		state = EngineStateReducer::connection(state, true);
		expect(state->connection == EngineConnectionState::Connected, "state model reports connected engine");

		std::vector<SessionSynthInfo> synths {
			{ "online", "Online Matrix", "Oberheim Matrix 1000", true, { true, true, false, false } },
			{ "offline", "Offline Matrix", "Oberheim Matrix 1000", false, { true, true, false, false } }
		};
		state = EngineStateReducer::synths(state, synths, {});
		expect(state->binding == BindingState::Unbound, "state model reports an unbound synth");
		state = EngineStateReducer::synths(state, synths, { "online", "Oberheim Matrix 1000" });
		expect(state->binding == BindingState::BoundOnline && state->canSend(), "explicit rebind resolves an online synth");
		state = EngineStateReducer::synths(state, synths, { "offline", "Oberheim Matrix 1000" });
		expect(state->binding == BindingState::BoundOffline, "state model distinguishes synth offline");
		state = EngineStateReducer::synths(state, synths, { "missing", "Oberheim Matrix 1000" });
		expect(state->binding == BindingState::Missing, "state model requests rebind for a missing synth");
		state = EngineStateReducer::synths(state, synths, { "online", "Sequential Prophet-6" });
		expect(state->binding == BindingState::AdaptationMismatch, "state model does not silently rebind an adaptation mismatch");
		state = EngineStateReducer::synths(state, synths, { "online", "Oberheim Matrix 1000" });

		auto transfer = TransferRecord {};
		transfer.status.transferId = "transfer";
		transfer.status.requestId = "request";
		for (auto transferState : { TransferState::Accepted, TransferState::Queued, TransferState::Preparing, TransferState::Sending,
			TransferState::Verifying }) {
			transfer.status.state = transferState;
			state = EngineStateReducer::transfer(state, transfer);
			expect(state->canCancel(), "queued/sending/verifying state remains cancellable");
		}
		transfer.status.state = TransferState::Succeeded;
		transfer.status.verification = VerificationState::Unverified;
		transfer.updatedAtUnixMillis = 100;
		state = EngineStateReducer::transfer(state, transfer);
		expect(!state->canCancel() && state->lastResultUnixMillis == 100, "sent state records a result timestamp");
		transfer.status.verification = VerificationState::Verified;
		state = EngineStateReducer::transfer(state, transfer);
		expect(state->transfer->status.verification == VerificationState::Verified, "verified is distinct from sent");
		transfer.status.state = TransferState::Failed;
		transfer.error = ServiceError { ServiceErrorCode::MidiPortBusy, "MIDI port busy", true };
		state = EngineStateReducer::transfer(state, transfer);
		expect(state->error && state->error->retryable, "failed state retains actionable retry information");
		transfer.status.state = TransferState::Cancelled;
		transfer.error = ServiceError { ServiceErrorCode::TransferCancelled, "Cancelled", false };
		state = EngineStateReducer::transfer(state, transfer);
		expect(state->transfer->status.state == TransferState::Cancelled, "cancelled state is represented explicitly");
		state = EngineStateReducer::failure(state, { ServiceErrorCode::ProtocolIncompatible, "Upgrade required", false });
		expect(state->connection == EngineConnectionState::ProtocolIncompatible, "protocol errors are visible as connection state");
		state = EngineStateReducer::connection(state, false);
		expect(state->connection == EngineConnectionState::Disconnected, "state model reports disconnected engine");
	}

	void testClientWorkflowAndReconnect() {
		using namespace knobkraft::recall;
		using namespace midikraft::session;
		auto const discoveryPath = testDiscoveryPath("integration");
		auto discovery = std::make_shared<DiscoveryFile>(discoveryPath);
		FakeSessionService service;
		SessionIpcServerConfig serverSettings;
		serverSettings.discoveryFile = discovery;
		serverSettings.tokenGenerator = [] { return "wp06-test-token-0123456789abcdef"; };
		serverSettings.staleClientTimeout = 500ms;
		SessionIpcServer firstServer(service, serverSettings);
		auto started = firstServer.start();
		expect(started.hasValue(), "fake Recall server starts for plugin UI integration test");
		if (!started) return;

		auto settings = testSettings(discoveryPath, "wp06-client");
		PluginProcessor processor(std::move(settings));
		expect(waitUntil([&] { return processor.engineClient().snapshot()->connection == EngineConnectionState::Connected; }),
			"processor-owned client connects to fake server");
		expect(waitUntil([&] { return processor.engineClient().snapshot()->synths.size() == 3; }),
			"configured synth picker receives server results");
		processor.engineClient().rebind("synth-matrix");
		expect(waitUntil([&] { return processor.engineClient().snapshot()->binding == BindingState::BoundOnline; }),
			"explicit rebind reaches bound-online state");
		processor.engineClient().searchPatches("warm");
		expect(waitUntil([&] { return processor.engineClient().snapshot()->patchResults.size() == 1; }),
			"paged patch search returns adaptation-compatible results");
		processor.engineClient().selectPatch("patch-warm-bass");
		expect(waitUntil([&] { return processor.state().snapshot()->manifest.sounds.front().patch.name == "Warm Bass"; }),
			"selected patch replaces the embedded project sound");
		expect(processor.state().snapshot()->manifest.recallPolicy == RecallPolicy::Manual,
			"only manual recall policy is persisted");

		std::unique_ptr<juce::AudioProcessorEditor> firstEditor(processor.createEditor());
		firstEditor.reset();
		expect(processor.engineClient().snapshot()->connection == EngineConnectionState::Connected,
			"closing the editor leaves the processor connection alive");
		std::unique_ptr<juce::AudioProcessorEditor> reopenedEditor(processor.createEditor());
		expect(reopenedEditor != nullptr, "editor can reopen over the existing client session");
		reopenedEditor.reset();

		processor.engineClient().sendStoredPatch();
		expect(waitUntil([&] { return processor.engineClient().snapshot()->transfer.has_value(); }),
			"manual Send reaches the fake edit-buffer queue");
		juce::MemoryBlock duringTransfer;
		processor.getStateInformation(duringTransfer);
		PluginState restored(PluginState::embeddedFixture("restored"));
		expect(restored.restore(std::span(static_cast<std::uint8_t const*>(duringTransfer.getData()), duringTransfer.getSize())),
			"project can save during an active transfer");
		expect(restored.snapshot()->manifest == processor.state().snapshot()->manifest,
			"save-during-transfer contains stable patch and binding, not transient progress");
		processor.engineClient().cancelTransfer();
		expect(waitUntil([&] {
			auto current = processor.engineClient().snapshot();
			return current->transfer && current->transfer->status.state == TransferState::Cancelled;
		}), "Cancel updates plugin state through the fake server");

		firstServer.stop();
		expect(waitUntil([&] { return processor.engineClient().snapshot()->connection == EngineConnectionState::Disconnected; }),
			"client reports engine shutdown without losing project state");
		SessionIpcServer restartedServer(service, serverSettings);
		auto restarted = restartedServer.start();
		expect(restarted.hasValue(), "fake Recall server restarts on a new generation");
		expect(waitUntil([&] { return processor.engineClient().snapshot()->connection == EngineConnectionState::Connected; }),
			"processor-owned client reconnects after KnobKraft restart");
		expect(processor.state().snapshot()->manifest.sounds.front().patch.name == "Warm Bass",
			"reconnect preserves the embedded project patch");
		restartedServer.stop();
	}
}

int main() {
	juce::ScopedJuceInitialiser_GUI initialiseJuce;
	testPassThrough<float>(juce::AudioChannelSet::mono(), "mono float audio is bit-identical");
	testPassThrough<float>(juce::AudioChannelSet::stereo(), "stereo float audio is bit-identical");
	testPassThrough<double>(juce::AudioChannelSet::mono(), "mono double audio is bit-identical");
	testPassThrough<double>(juce::AudioChannelSet::stereo(), "stereo double audio is bit-identical");
	testStateRoundTrip();
	testCorruptStatePreservation();
	testFutureStatePreservation();
	testProcessorStateDuplication();
	testProcessorPersistsManualPolicyOnly();
	testInvalidEditDoesNotEraseState();
	testEngineStateModel();
	testClientWorkflowAndReconnect();
	if (failures == 0) std::cout << "All KnobKraft Recall tests passed\n";
	return failures == 0 ? 0 : 1;
}
