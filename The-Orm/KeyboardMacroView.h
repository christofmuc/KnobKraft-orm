/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "JuceHeader.h"

#include "MidiController.h"

class KeyboardMacroView : public Component {
public:
	KeyboardMacroView();
	virtual ~KeyboardMacroView();

	virtual void resized() override;

private:
	MidiKeyboardState state_;
	MidiKeyboardComponent keyboard_;

	midikraft::MidiController::HandlerHandle handle_ = midikraft::MidiController::makeNoneHandle();
};

