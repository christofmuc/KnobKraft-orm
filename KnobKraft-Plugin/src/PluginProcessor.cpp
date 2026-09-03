/*
   Copyright (c) 2026 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "PluginProcessor.h"

#include "PluginEditor.h"

#include <juce_core/juce_core.h>

#include <cstdint>
#include <span>

namespace knobkraft::recall {

	PluginProcessor::PluginProcessor() : PluginProcessor(defaultEngineClientSettings()) {
	}

	PluginProcessor::PluginProcessor(EngineClientSettings engineSettings)
		: AudioProcessor(BusesProperties()
			.withInput("Input", juce::AudioChannelSet::stereo(), true)
			.withOutput("Output", juce::AudioChannelSet::stereo(), true)),
		  state_(PluginState::embeddedFixture(juce::Uuid().toString().toStdString())),
		  engineClient_(state_, std::move(engineSettings)) {
		setLatencySamples(0);
	}

	void PluginProcessor::prepareToPlay(double, int) {
		setLatencySamples(0);
	}

	void PluginProcessor::releaseResources() {
	}

	bool PluginProcessor::isBusesLayoutSupported(BusesLayout const& layouts) const {
		auto const& input = layouts.getMainInputChannelSet();
		auto const& output = layouts.getMainOutputChannelSet();
		return input == output && (input == juce::AudioChannelSet::mono() || input == juce::AudioChannelSet::stereo());
	}

	template<typename Sample>
	void PluginProcessor::passThrough(juce::AudioBuffer<Sample>&) noexcept {
		// JUCE supplies an in-place buffer. Matching bus layouts guarantee that
		// doing nothing is exact pass-through and introduces no audio-thread work.
	}

	void PluginProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
		juce::ScopedNoDenormals noDenormals;
		passThrough(buffer);
	}

	void PluginProcessor::processBlock(juce::AudioBuffer<double>& buffer, juce::MidiBuffer&) {
		juce::ScopedNoDenormals noDenormals;
		passThrough(buffer);
	}

	juce::AudioProcessorEditor* PluginProcessor::createEditor() {
		return new PluginEditor(*this);
	}

	void PluginProcessor::setCurrentProgram(int) {
	}

	juce::String const PluginProcessor::getProgramName(int) {
		return {};
	}

	void PluginProcessor::changeProgramName(int, juce::String const&) {
	}

	void PluginProcessor::getStateInformation(juce::MemoryBlock& destinationData) {
		try {
			auto const serialized = state_.serialize();
			destinationData.replaceAll(serialized.data(), serialized.size());
		}
		catch (...) {
			// Never let an allocation failure cross the plugin/host ABI.
			destinationData.reset();
		}
	}

	void PluginProcessor::setStateInformation(void const* data, int sizeInBytes) {
		if (data == nullptr || sizeInBytes < 0) {
			state_.restore({});
			return;
		}
		auto const* first = static_cast<std::uint8_t const*>(data);
		if (state_.restore(std::span<std::uint8_t const>(first, static_cast<std::size_t>(sizeInBytes)))) {
			auto manifest = state_.snapshot()->manifest;
			// WP-06 deliberately implements manual recall only. Do not let a state
			// value for a future policy acquire automatic behavior in this binary.
			manifest.recallPolicy = midikraft::session::RecallPolicy::Manual;
			state_.replaceManifest(std::move(manifest));
		}
		engineClient_.manifestChanged();
	}

}
