/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "JuceHeader.h"

#include "MidiController.h"

#include <set>

struct KeyboardMacro {
	std::set<int> midiNotes;
	std::function<void()> execute;
};

class KeyboardMacroView : public Component {
public:
	KeyboardMacroView();
	virtual ~KeyboardMacroView();

	virtual void resized() override;

private:
	bool isMacroState(KeyboardMacro const &macro);

	MidiKeyboardState state_;
	MidiKeyboardComponent keyboard_;

	std::vector<KeyboardMacro> macros_;

	midikraft::MidiController::HandlerHandle handle_ = midikraft::MidiController::makeNoneHandle();
};

