/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "JuceHeader.h"

#include "MouseUpAndDownButton.h"

#include <set>

enum class KeyboardMacroEvent {
	Hide,
	Favorite,
	Regular,
	PreviousPatch,
	NextPatch,
	ImportEditBuffer,
	Unknown	
};

extern std::vector<KeyboardMacroEvent> kAllKeyboardMacroEvents;

struct KeyboardMacro {
	KeyboardMacroEvent event;
	std::set<int> midiNotes;

	static std::string toText(KeyboardMacroEvent event);
	static KeyboardMacroEvent fromText(std::string const &text);
};

class MacroConfig : public Component,
	private TextButton::Listener
{
public:
	MacroConfig(KeyboardMacroEvent event, std::function<void(KeyboardMacroEvent)> recordHander, std::function<void(KeyboardMacroEvent, bool)> showHandler);

	virtual void resized() override;

	void setData(KeyboardMacro const &macro);

private:
	void buttonClicked(Button* button) override;
	void buttonStateChanged(Button* button) override;

	KeyboardMacroEvent event_;
	std::function<void(KeyboardMacroEvent)> recordHander_;
	std::function<void(KeyboardMacroEvent, bool)> showHandler_;
	Label name_;
	Label keyList_;
	TextButton record_;
	MouseUpAndDownButton play_;
};


