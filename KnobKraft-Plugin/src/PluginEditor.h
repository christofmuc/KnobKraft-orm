/*
   Copyright (c) 2026 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>

namespace knobkraft::recall {

	class PluginProcessor;

	class PluginEditor final : public juce::AudioProcessorEditor, private juce::Timer {
	public:
		explicit PluginEditor(PluginProcessor& processor);

		void paint(juce::Graphics& graphics) override;
		void resized() override;

	private:
		void timerCallback() override;
		void refresh();

		PluginProcessor& processor_;
		juce::Label title_;
		juce::Label engineStatus_;
		juce::Label instanceName_;
		juce::Label patchName_;
		juce::Label fingerprint_;
		juce::Label storedStatus_;
		juce::Label stateError_;

		JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditor)
	};

}
