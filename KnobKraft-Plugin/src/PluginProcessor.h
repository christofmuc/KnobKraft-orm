/*
   Copyright (c) 2026 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "EngineClient.h"
#include "PluginState.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace knobkraft::recall {

	class PluginProcessor final : public juce::AudioProcessor {
	public:
		PluginProcessor();
		explicit PluginProcessor(EngineClientSettings engineSettings);

		void prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock) override;
		void releaseResources() override;
		bool isBusesLayoutSupported(BusesLayout const& layouts) const override;
		void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;
		void processBlock(juce::AudioBuffer<double>& buffer, juce::MidiBuffer& midiMessages) override;

		[[nodiscard]] juce::AudioProcessorEditor* createEditor() override;
		[[nodiscard]] bool hasEditor() const override { return true; }
		[[nodiscard]] juce::String const getName() const override { return "KnobKraft Recall"; }
		[[nodiscard]] bool acceptsMidi() const override { return false; }
		[[nodiscard]] bool producesMidi() const override { return false; }
		[[nodiscard]] bool isMidiEffect() const override { return false; }
		[[nodiscard]] double getTailLengthSeconds() const override { return 0.0; }
		[[nodiscard]] bool supportsDoublePrecisionProcessing() const override { return true; }
		[[nodiscard]] int getNumPrograms() override { return 1; }
		[[nodiscard]] int getCurrentProgram() override { return 0; }
		void setCurrentProgram(int index) override;
		[[nodiscard]] juce::String const getProgramName(int index) override;
		void changeProgramName(int index, juce::String const& newName) override;

		void getStateInformation(juce::MemoryBlock& destinationData) override;
		void setStateInformation(void const* data, int sizeInBytes) override;

		[[nodiscard]] PluginState& state() noexcept { return state_; }
		[[nodiscard]] PluginState const& state() const noexcept { return state_; }
		[[nodiscard]] EngineClient& engineClient() noexcept { return engineClient_; }
		[[nodiscard]] EngineClient const& engineClient() const noexcept { return engineClient_; }

	private:
		template<typename Sample>
		void passThrough(juce::AudioBuffer<Sample>& buffer) noexcept;

		PluginState state_;
		EngineClient engineClient_;

		JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginProcessor)
	};

}
