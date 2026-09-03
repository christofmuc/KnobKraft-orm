/*
   Copyright (c) 2026 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>

namespace knobkraft::recall {

	class PluginProcessor;

	class PluginEditor final : public juce::AudioProcessorEditor, private juce::ChangeListener {
	public:
		explicit PluginEditor(PluginProcessor& processor);
		~PluginEditor() override;

		void paint(juce::Graphics& graphics) override;
		void resized() override;

	private:
		void changeListenerCallback(juce::ChangeBroadcaster* source) override;
		void refresh();
		void commitInstanceName();

		PluginProcessor& processor_;
		juce::Label title_;
		juce::Label engineStatus_;
		juce::TextEditor instanceName_;
		juce::TextButton openKnobKraft_ { "Open KnobKraft" };
		juce::Label bindingStatus_;
		juce::ComboBox synthPicker_;
		juce::TextButton rebind_ { "Rebind" };
		juce::TextEditor patchSearch_;
		juce::TextButton search_ { "Search" };
		juce::ComboBox patchPicker_;
		juce::TextButton choosePatch_ { "Choose" };
		juce::TextButton morePatches_ { "More" };
		juce::Label patchName_;
		juce::Label fingerprint_;
		juce::Label provenance_;
		juce::Label storedStatus_;
		juce::TextButton send_ { "Send to synth" };
		juce::TextButton cancel_ { "Cancel" };
		juce::ProgressBar progress_;
		double progressValue_ = 0.0;
		juce::Label transferStatus_;
		juce::Label recallPolicy_;
		juce::Label stateError_;

		JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditor)
	};

}
