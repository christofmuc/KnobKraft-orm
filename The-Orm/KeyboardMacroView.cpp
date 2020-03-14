/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "KeyboardMacroView.h"

#include "Logger.h"

KeyboardMacroView::KeyboardMacroView() : keyboard_(state_, MidiKeyboardComponent::horizontalKeyboard)
{
	addAndMakeVisible(keyboard_);

	keyboard_.setAvailableRange(0x24, 0x60);

	// Install keyboard handler to refresh midi keyboard display
	midikraft::MidiController::instance()->addMessageHandler(handle_, [this](MidiInput *source, MidiMessage const &message) {
		if (message.isNoteOnOrOff()) {
			ignoreUnused(source);
			state_.processNextMidiEvent(message);

			// Check if this is a message we will transform into a macro
			for (const auto& macro : macros_) {
				if (isMacroState(macro)) {
					macro.execute();
				}
			}
		}
	});

	// Define a macro
	macros_ = {
		{{ 0x24 }, [this]() {
			SimpleLogger::instance()->postMessage("Macro executed");
		} }
	};
}

KeyboardMacroView::~KeyboardMacroView()
{
	midikraft::MidiController::instance()->removeMessageHandler(handle_);
}

void KeyboardMacroView::resized()
{
	auto area = getLocalBounds();

	// Needed with
	float keyboardDesiredWidth = keyboard_.getTotalKeyboardWidth() + 16;

	keyboard_.setBounds(area.withSizeKeepingCentre((int) keyboardDesiredWidth, std::min(area.getHeight(), 150)).reduced(8));
	
}

bool KeyboardMacroView::isMacroState(KeyboardMacro const &macro)
{
	// Check if the keyboard state does contain all keys of the keyboard macro
	bool allDetected = true;
	for (int note : macro.midiNotes) {
		if (!state_.isNoteOnForChannels(0xf, note)) {
			// No, this note is missing
			allDetected = false;
			break;
		}
	}
	// Check that no extra key is pressed
	bool extraKeyDetected = false;
	for (int note = 0; note < 128; note++) {
		if (state_.isNoteOnForChannels(0xf, note)) {
			if (macro.midiNotes.find(note) == macro.midiNotes.end()) {
				extraKeyDetected = true;
				break;
			}
		}
	}
	return allDetected && !extraKeyDetected;
}
