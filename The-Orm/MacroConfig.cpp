/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "MacroConfig.h"

#include "MidiNote.h"

// This is required because you cannot loop over the new enum class type in C++
// Gives me the advantage to not list KeyboardMacroevent::Unkown, so it is already useful for the UI
std::vector<KeyboardMacroEvent> kAllKeyboardMacroEvents = {
	KeyboardMacroEvent::Hide,
	KeyboardMacroEvent::Favorite,
	KeyboardMacroEvent::PreviousPatch,
	KeyboardMacroEvent::NextPatch,
	KeyboardMacroEvent::ImportEditBuffer
};

std::string KeyboardMacro::toText(KeyboardMacroEvent event)
{
	switch (event) {
	case KeyboardMacroEvent::Hide: return "Hide"; 
	case KeyboardMacroEvent::Favorite: return "Favorite";
	case KeyboardMacroEvent::PreviousPatch: return "PreviousPatch"; 
	case KeyboardMacroEvent::NextPatch: return "NextPatch"; 
	case KeyboardMacroEvent::ImportEditBuffer: return "ImportEditBuffer";
    case KeyboardMacroEvent::Unknown:
        // fallthrough
	default:
		return "Unknown";
	}
}

KeyboardMacroEvent KeyboardMacro::fromText(std::string const &event)
{
	if ("Hide" == event) {
		return KeyboardMacroEvent::Hide;
	}
	else if ("Favorite" == event) {
		return KeyboardMacroEvent::Favorite;
	}
	else if ("PreviousPatch" == event) {
		return  KeyboardMacroEvent::PreviousPatch;
	}
	else if ("NextPatch" == event) {
		return KeyboardMacroEvent::NextPatch;
	}
	else if ("ImportEditBuffer" == event) {
		return KeyboardMacroEvent::ImportEditBuffer;
	}
	return KeyboardMacroEvent::Unknown;
}

MacroConfig::MacroConfig(KeyboardMacroEvent event, 
	std::function<void(KeyboardMacroEvent)> recordHander,
	std::function<void(KeyboardMacroEvent, bool)> showHandler) : event_(event), 
	recordHander_(recordHander),
	showHandler_(showHandler), play_([this](TextButton *button) { buttonStateChanged(button);  }) // NOLINT
{
	addAndMakeVisible(name_);
	name_.setText(KeyboardMacro::toText(event_), dontSendNotification);
	addAndMakeVisible(keyList_);
	addAndMakeVisible(record_);
	record_.setButtonText("Record keys");
	record_.addListener(this);
	addAndMakeVisible(play_);
	play_.setButtonText("Show keys");
	play_.addListener(this);
}

void MacroConfig::resized()
{
	auto area = getLocalBounds();
	name_.setBounds(area.removeFromLeft(100));
	play_.setBounds(area.removeFromRight(100));
	record_.setBounds(area.removeFromRight(100).withTrimmedRight(8));
	keyList_.setBounds(area.withTrimmedLeft(8).withTrimmedRight(8));
}

void MacroConfig::setData(KeyboardMacro const &macro)
{
	name_.setText(KeyboardMacro::toText(macro.event), dontSendNotification);
	String notes;
	for (auto note : macro.midiNotes) {
		MidiNote n(note);
		if (notes.isNotEmpty()) {
			notes += ", ";
		}
		notes += String(n.name());
	}
	keyList_.setText(notes, dontSendNotification);
}

void MacroConfig::buttonStateChanged(Button *button)
{
	if (button && button == &play_) {
		showHandler_(event_, button->isDown());
	}
}

void MacroConfig::buttonClicked(Button *button)
{
	if (button == &record_) {
		recordHander_(event_);
	}
}

