/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "JuceHeader.h"

#include "MidiController.h"

#include <set>

enum class KeyboardMacroEvent {
	Hide,
	Favorite,
	PreviousPatch,
	NextPatch,
	ImportEditBuffer,
	Unknown
};

struct KeyboardMacro {
	std::set<int> midiNotes;
};

class KeyboardMacroView : public Component {
public:
	KeyboardMacroView(std::function<void(KeyboardMacroEvent)> callback);
	virtual ~KeyboardMacroView();

	virtual void resized() override;

private:
	void loadFromSettings();
	void saveSettings();
	bool isMacroState(KeyboardMacro const &macro);

	MidiKeyboardState state_;
	MidiKeyboardComponent keyboard_;

	std::map<KeyboardMacroEvent, KeyboardMacro> macros_;
	std::function<void(KeyboardMacroEvent)> executeMacro_;

	midikraft::MidiController::HandlerHandle handle_ = midikraft::MidiController::makeNoneHandle();
};

