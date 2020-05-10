/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "JuceHeader.h"

#include "MidiController.h"
#include "PropertyEditor.h"
#include "MacroConfig.h"

class KeyboardMacroView : public Component, private ChangeListener, private Value::Listener {
public:
	KeyboardMacroView(std::function<void(KeyboardMacroEvent)> callback);
	virtual ~KeyboardMacroView();

	virtual void resized() override;

private:
	void setupPropertyEditor();
	void setupKeyboardControl();
	void loadFromSettings();
	void saveSettings();
	bool isMacroState(KeyboardMacro const &macro);
	void refreshUI();

	void changeListenerCallback(ChangeBroadcaster* source) override; // This gets called when the synth is changed
	void valueChanged(Value& value) override; // This gets called when the property editor is used

	PropertyEditor customSetup_;
	MidiKeyboardState state_;
	MidiKeyboardComponent keyboard_;

	OwnedArray<MacroConfig> configs_;

	std::map<KeyboardMacroEvent, KeyboardMacro> macros_;
	std::function<void(KeyboardMacroEvent)> executeMacro_;

	midikraft::MidiController::HandlerHandle handle_ = midikraft::MidiController::makeNoneHandle();

	TypedNamedValueSet customMasterkeyboardSetup_;
};

