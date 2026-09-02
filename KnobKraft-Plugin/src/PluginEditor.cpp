/*
   Copyright (c) 2026 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "PluginEditor.h"

#include "PluginProcessor.h"

namespace knobkraft::recall {

	namespace {
		void configureLabel(juce::Component& parent, juce::Label& label, float fontSize, juce::Colour colour) {
			label.setFont(juce::Font(juce::FontOptions(fontSize)));
			label.setColour(juce::Label::textColourId, colour);
			label.setJustificationType(juce::Justification::centredLeft);
			parent.addAndMakeVisible(label);
		}
	}

	PluginEditor::PluginEditor(PluginProcessor& processor)
		: AudioProcessorEditor(processor), processor_(processor) {
		configureLabel(*this, title_, 24.0f, juce::Colours::white);
		configureLabel(*this, engineStatus_, 14.0f, juce::Colour(0xffaeb8c4));
		configureLabel(*this, instanceName_, 17.0f, juce::Colours::white);
		configureLabel(*this, patchName_, 19.0f, juce::Colour(0xff75d7b0));
		configureLabel(*this, fingerprint_, 12.0f, juce::Colour(0xff8d99a8));
		configureLabel(*this, storedStatus_, 14.0f, juce::Colour(0xff75d7b0));
		configureLabel(*this, stateError_, 13.0f, juce::Colour(0xffff8d8d));
		title_.setText("KnobKraft Recall", juce::dontSendNotification);
		engineStatus_.setText("Engine  •  Disconnected", juce::dontSendNotification);
		setSize(560, 310);
		refresh();
		startTimerHz(4);
	}

	void PluginEditor::paint(juce::Graphics& graphics) {
		graphics.fillAll(juce::Colour(0xff14191f));
		graphics.setColour(juce::Colour(0xff2a333d));
		graphics.fillRoundedRectangle(getLocalBounds().toFloat().reduced(16.0f).withTrimmedTop(78.0f), 8.0f);
	}

	void PluginEditor::resized() {
		auto bounds = getLocalBounds().reduced(24);
		title_.setBounds(bounds.removeFromTop(34));
		engineStatus_.setBounds(bounds.removeFromTop(30));
		bounds.removeFromTop(26);
		instanceName_.setBounds(bounds.removeFromTop(30));
		patchName_.setBounds(bounds.removeFromTop(34));
		fingerprint_.setBounds(bounds.removeFromTop(28));
		storedStatus_.setBounds(bounds.removeFromTop(30));
		stateError_.setBounds(bounds.removeFromTop(48));
	}

	void PluginEditor::timerCallback() {
		refresh();
	}

	void PluginEditor::refresh() {
		auto const snapshot = processor_.state().snapshot();
		auto const& manifest = snapshot->manifest;
		instanceName_.setText("Instance  •  " + juce::String(manifest.instanceName), juce::dontSendNotification);

		juce::String patchName = "No embedded project sound";
		juce::String fingerprint;
		if (manifest.selectedSoundId) {
			for (auto const& sound : manifest.sounds) {
				if (sound.soundId == *manifest.selectedSoundId) {
					patchName = juce::String(sound.patch.name);
					fingerprint = juce::String(sound.patch.fingerprint);
					break;
				}
			}
		}
		patchName_.setText("Project sound  •  " + patchName, juce::dontSendNotification);
		fingerprint_.setText(fingerprint.isEmpty() ? juce::String() : "Fingerprint  •  " + fingerprint, juce::dontSendNotification);
		storedStatus_.setText(snapshot->serializedState.empty() ? "Not stored in project" : "✓ Stored in project", juce::dontSendNotification);

		if (snapshot->decodeError) {
			auto const& error = *snapshot->decodeError;
			stateError_.setText("State error at " + juce::String(error.path) + ": " + juce::String(error.message)
				+ (error.rawStatePreserved ? " (raw state preserved)" : ""), juce::dontSendNotification);
		}
		else {
			stateError_.setText("Manual recall only • No MIDI or hardware access in this shell", juce::dontSendNotification);
		}
	}

}
