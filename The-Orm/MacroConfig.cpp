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
	KeyboardMacroEvent::Regular,
	KeyboardMacroEvent::PreviousPatch,
	KeyboardMacroEvent::NextPatch,
	KeyboardMacroEvent::ImportEditBuffer
};

std::string KeyboardMacro::toText(KeyboardMacroEvent event)
{
	switch (event) {
	case KeyboardMacroEvent::Hide: return "Hide"; 
	case KeyboardMacroEvent::Favorite: return "Favorite";
	case KeyboardMacroEvent::Regular: return "Regular";
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
	else if ("Regular" == event) {
		return KeyboardMacroEvent::Regular;
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
	std::function<void(KeyboardMacroEvent)> keyboardRecordHandler,
	std::function<void(KeyboardMacroEvent, bool)> showHandler) : event_(event),
	recordHander_(recordHander),
	keyboardRecordHandler_(keyboardRecordHandler),
	showHandler_(showHandler), play_([this](TextButton *button) { buttonStateChanged(button);  }) // NOLINT
{
	addAndMakeVisible(name_);
	name_.setText(KeyboardMacro::toText(event_), dontSendNotification);
	addAndMakeVisible(keyList_);
	addAndMakeVisible(keyboardKey_);
	addAndMakeVisible(record_);
	record_.setButtonText("Assign MIDI");
	record_.addListener(this);
	addAndMakeVisible(keyboardRecord_);
	keyboardRecord_.setButtonText("Assign key");
	keyboardRecord_.addListener(this);
	addAndMakeVisible(play_);
	play_.setButtonText("Show MIDI");
	play_.addListener(this);
	setKeyboardData(0);
}

void MacroConfig::resized()
{
	auto area = getLocalBounds();
	name_.setBounds(area.removeFromLeft(110));
	play_.setBounds(area.removeFromRight(90));
	keyboardRecord_.setBounds(area.removeFromRight(90).withTrimmedRight(8));
	record_.setBounds(area.removeFromRight(90).withTrimmedRight(8));

	auto textArea = area.withTrimmedLeft(8).withTrimmedRight(8);
	auto midiLine = textArea.removeFromTop(textArea.getHeight() / 2);
	keyList_.setBounds(midiLine);
	keyboardKey_.setBounds(textArea);
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
	if (notes.isEmpty()) {
		notes = "-";
	}
	keyList_.setText("MIDI: " + notes, dontSendNotification);
}

void MacroConfig::setKeyboardData(int keyCode)
{
	if (keyCode > 0) {
		keyboardKey_.setText("Key: " + juce::KeyPress(keyCode).getTextDescription(), dontSendNotification);
	}
	else {
		keyboardKey_.setText("Key: -", dontSendNotification);
	}
}

void MacroConfig::setKeyboardAssignmentPending(bool pending)
{
	keyboardRecord_.setButtonText(pending ? "Press key..." : "Assign key");
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
	else if (button == &keyboardRecord_) {
		keyboardRecordHandler_(event_);
	}
}
