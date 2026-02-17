/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "JuceHeader.h"

#include "MidiController.h"
#include "PropertyEditor.h"
#include "MacroConfig.h"

#include "MidiChannelPropertyEditor.h"
#include "ElectraOneRouter.h"

#include <mutex>

class KeyboardMacroView : public Component, private ChangeListener, private Value::Listener {
public:
	KeyboardMacroView(std::function<void(KeyboardMacroEvent)> executeCallback,
		std::function<void(KeyboardMacroEvent, int, bool)> assignKeyboardShortcutCallback,
		std::function<int(KeyboardMacroEvent)> getKeyboardShortcutCallback);
	virtual ~KeyboardMacroView() override;

	virtual void resized() override;
	void handleMidiMessage(const MidiMessage& message, const String& source, bool isOut);

private:
	class RecordProgress;
	class KeyboardRecordProgress;

	void setupPropertyEditor();
	void setupKeyboardControl();
	void loadFromSettings();
	void saveSettings();
	void refreshSecondaryMidiOutList();
	void updateSecondaryMidiOutSelection();
	bool isMacroState(KeyboardMacro const &macro);
	void refreshSynthList();
	void refreshUI();

	void turnOnMasterkeyboardInput();

	void changeListenerCallback(ChangeBroadcaster* source) override; // This gets called when the synth is changed
	void valueChanged(Value& value) override; // This gets called when the property editor is used

	PropertyEditor customSetup_;
	MidiKeyboardState state_;
	MidiKeyboardComponent keyboard_;
	Viewport macroViewport_;
	std::unique_ptr<Component> macroContainer_;
	std::shared_ptr<MidiDevicePropertyEditor> midiDeviceList_; // Listen to this to get notified of newly available devices!
	std::shared_ptr<MidiDevicePropertyEditor> secondaryMidiOutList_;
	ElectraOneRouter controllerRouter_;
	std::shared_ptr<TypedNamedValue> synthListEditor_;

	OwnedArray<MacroConfig> configs_;

	std::map<KeyboardMacroEvent, KeyboardMacro> macros_;
	KeyboardMacroEvent pendingKeyboardAssignment_ = KeyboardMacroEvent::Unknown;
	std::function<void(KeyboardMacroEvent)> executeMacro_;
	std::function<void(KeyboardMacroEvent, int, bool)> assignKeyboardShortcut_;
	std::function<int(KeyboardMacroEvent)> getKeyboardShortcut_;
	std::map<KeyboardMacroEvent, bool> macroActiveStates_; // Tracks edge-trigger state to avoid repeats while held

	midikraft::MidiController::HandlerHandle handle_ = midikraft::MidiController::makeNoneHandle();

	TypedNamedValueSet customMasterkeyboardSetup_;
	std::shared_ptr<RecordProgress> activeRecorder_; // Should have maximum one active macro recorders open
	std::shared_ptr<KeyboardRecordProgress> activeKeyboardRecorder_;

	std::mutex secondaryMidiOutMutex_;
	juce::MidiDeviceInfo secondaryMidiOut_;
};
