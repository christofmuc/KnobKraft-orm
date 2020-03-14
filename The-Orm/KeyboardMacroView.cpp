/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "KeyboardMacroView.h"


KeyboardMacroView::KeyboardMacroView() : keyboard_(state_, MidiKeyboardComponent::horizontalKeyboard)
{
	addAndMakeVisible(keyboard_);

	keyboard_.setAvailableRange(0x24, 0x60);

	// Install keyboard handler to refresh midi keyboard display
	midikraft::MidiController::instance()->addMessageHandler(handle_, [this](MidiInput *source, MidiMessage const &message) {
		ignoreUnused(source);
		state_.processNextMidiEvent(message);
	});
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
