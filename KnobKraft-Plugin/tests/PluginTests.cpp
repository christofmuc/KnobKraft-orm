/*
   Copyright (c) 2026 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "PluginProcessor.h"
#include "PluginState.h"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace {
	int failures = 0;

	void expect(bool condition, char const* description) {
		if (!condition) {
			std::cerr << "FAILED: " << description << '\n';
			++failures;
		}
	}

	template<typename Sample>
	void testPassThrough(juce::AudioChannelSet layout, char const* description) {
		knobkraft::recall::PluginProcessor processor;
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
		knobkraft::recall::PluginProcessor source;
		juce::MemoryBlock state;
		source.getStateInformation(state);
		knobkraft::recall::PluginProcessor duplicate;
		duplicate.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
		expect(duplicate.state().snapshot()->manifest == source.state().snapshot()->manifest, "processor duplication restores embedded state");
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
}

int main() {
	testPassThrough<float>(juce::AudioChannelSet::mono(), "mono float audio is bit-identical");
	testPassThrough<float>(juce::AudioChannelSet::stereo(), "stereo float audio is bit-identical");
	testPassThrough<double>(juce::AudioChannelSet::mono(), "mono double audio is bit-identical");
	testPassThrough<double>(juce::AudioChannelSet::stereo(), "stereo double audio is bit-identical");
	testStateRoundTrip();
	testCorruptStatePreservation();
	testFutureStatePreservation();
	testProcessorStateDuplication();
	testInvalidEditDoesNotEraseState();
	if (failures == 0) std::cout << "All KnobKraft Recall tests passed\n";
	return failures == 0 ? 0 : 1;
}
