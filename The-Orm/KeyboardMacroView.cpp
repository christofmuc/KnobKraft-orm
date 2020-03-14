/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "KeyboardMacroView.h"

#include "Logger.h"

#include "Settings.h"

KeyboardMacroView::KeyboardMacroView(std::function<void(KeyboardMacroEvent)> callback) : keyboard_(state_, MidiKeyboardComponent::horizontalKeyboard), executeMacro_(callback)
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
				if (isMacroState(macro.second)) {
					executeMacro_(macro.first);
				}
			}
		}
	});

	// Load Macro Definitions
	loadFromSettings();
	/*macros_ = {
		{ KeyboardMacroEvent::PreviousPatch, {{ 0x24 }} }
	};*/
}

KeyboardMacroView::~KeyboardMacroView()
{
	midikraft::MidiController::instance()->removeMessageHandler(handle_);
	saveSettings();
}

void KeyboardMacroView::loadFromSettings() {
	auto json = Settings::instance().get("MacroDefinitions");
	var macros = JSON::parse(String(json));

	if (macros.isArray()) {
		for (var macro : *macros.getArray()) {
			if (macro.isObject()) {
				std::set<int> midiNoteValues;
				auto notes = macro.getProperty("Notes", var());
				if (notes.isArray()) {
					for (var noteVar : *notes.getArray()) {
						if (noteVar.isInt()) {
							midiNoteValues.insert((int)noteVar);
						}
					}
				}
				auto event = macro.getProperty("Event", var());
				KeyboardMacroEvent macroEventCode = KeyboardMacroEvent::Unknown;
				if (event.isString()) {
					if ("Hide" == event) {
						macroEventCode = KeyboardMacroEvent::Hide;
					}
					else if ("Favorite" == event) {
						macroEventCode = KeyboardMacroEvent::Favorite;
					}
					else if ("PreviousPatch" == event) {
						macroEventCode = KeyboardMacroEvent::PreviousPatch;
					}
					else if ("NextPatch" == event) {
						macroEventCode = KeyboardMacroEvent::NextPatch;
					}
					else if ("ImportEditBuffer" == event) {
						macroEventCode = KeyboardMacroEvent::ImportEditBuffer;
					}
				}
				if (macroEventCode != KeyboardMacroEvent::Unknown && !midiNoteValues.empty()) {
					macros_[macroEventCode] = { midiNoteValues };
				}
			}
		}
	}
}

void KeyboardMacroView::saveSettings() {
	var result;
	
	for (auto macro : macros_) {
		String event;
		switch (macro.first) {
		case KeyboardMacroEvent::Hide: event = "Hide"; break;
		case KeyboardMacroEvent::Favorite: event = "Favorite"; break;
		case KeyboardMacroEvent::PreviousPatch: event = "PreviousPatch"; break;
		case KeyboardMacroEvent::NextPatch: event = "NextPatch"; break;
		case KeyboardMacroEvent::ImportEditBuffer: event = "ImportEditBuffer"; break;
		}
		var notes;
		for (auto note : macro.second.midiNotes) {
			notes.append(note);
		}
		auto def = new DynamicObject();
		def->setProperty("Notes", notes);
		def->setProperty("Event", event);
		result.append(def);
	}
	String json = JSON::toString(result);
	Settings::instance().set("MacroDefinitions", json.toStdString());	
	Settings::instance().flush();
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
