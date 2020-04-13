/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "KeyboardMacroView.h"

#include "Logger.h"

#include "Settings.h"

class RecordProgress : public ThreadWithProgressWindow, private MidiKeyboardStateListener {
public:
	RecordProgress(MidiKeyboardState &state) : ThreadWithProgressWindow("Press key(s) on your MIDI keyboard", false, true), state_(state), atLeastOneKey_(false), done_(false)
	{
	}

	virtual ~RecordProgress() {
		state_.removeListener(this);
	}

	virtual void run() override {
		state_.addListener(this);
		while (!threadShouldExit() && !done_) {
			Thread::sleep(10);
		}
	}

	virtual void handleNoteOn(MidiKeyboardState* source, int midiChannel, int midiNoteNumber, float velocity) override {
		ignoreUnused(source, midiChannel, velocity);
		notes_.insert(midiNoteNumber);
		atLeastOneKey_ = true;
	}

	virtual void handleNoteOff(MidiKeyboardState* source, int midiChannel, int midiNoteNumber, float velocity) override {
		ignoreUnused(source, midiChannel, midiNoteNumber, velocity);
		int keyPressed = 0;
		for (int i = 0; i < 128; i++) {
			if (state_.isNoteOnForChannels(0xffff, i)) {
				keyPressed++;
			}
		}
		if (keyPressed == 0) {
			done_ = true; 
		}
	}

	std::set<int> notesSelected() { return notes_; }

private:
	std::set<int> notes_;
	MidiKeyboardState &state_;
	bool atLeastOneKey_;
	bool done_;

};

KeyboardMacroView::KeyboardMacroView(std::function<void(KeyboardMacroEvent)> callback) : keyboard_(state_, MidiKeyboardComponent::horizontalKeyboard), executeMacro_(callback)
{
	addAndMakeVisible(keyboard_);
	keyboard_.setAvailableRange(0x24, 0x60);
	keyboard_.setOctaveForMiddleC(4);

	// Create config table
	for (auto config : kAllKeyboardMacroEvents) {
		auto configComponent = new MacroConfig(config,
			[this](KeyboardMacroEvent event) {
				RecordProgress recorder(state_);
				if (recorder.runThread()) {
					KeyboardMacro newMacro = { event, recorder.notesSelected() };
					macros_[event] = newMacro;
					saveSettings();
					refreshUI();
				}
			},
			[this](KeyboardMacroEvent event, bool down) {
			if (macros_.find(event) != macros_.end()) {
				for (auto key : macros_[event].midiNotes) {
					if (down) {
						state_.noteOn(1, key, 1.0f);
					}
					else {
						state_.noteOff(1, key, 1.0f);
					}
				}
			}
		});
		configs_.add(configComponent);
		addAndMakeVisible(configComponent);
	}

	// Install keyboard handler to refresh midi keyboard display
	midikraft::MidiController::instance()->addMessageHandler(handle_, [this](MidiInput *source, MidiMessage const &message) {
		if (message.isNoteOnOrOff()) {
			ignoreUnused(source);
			state_.processNextMidiEvent(message);

			// Check if this is a message we will transform into a macro
			for (const auto& macro : macros_) {
				if (isMacroState(macro.second)) {
					auto code = macro.first;
					MessageManager::callAsync([this, code]() {
						executeMacro_(code);
					});
				}
			}
		}
	});

	// Load Macro Definitions
	loadFromSettings();
	refreshUI();
}

KeyboardMacroView::~KeyboardMacroView()
{
	midikraft::MidiController::instance()->removeMessageHandler(handle_);
	saveSettings();
}

void KeyboardMacroView::refreshUI() {
	// Set UI
	int i = 0;
	for (auto config : kAllKeyboardMacroEvents) {
		if (macros_.find(config) != macros_.end()) {
			configs_[i]->setData(macros_[config]);
		}
		i++;
	}
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
					String eventString = event;
					macroEventCode = KeyboardMacro::fromText(eventString.toStdString());
				}
				if (macroEventCode != KeyboardMacroEvent::Unknown && !midiNoteValues.empty()) {
					macros_[macroEventCode] = { macroEventCode, midiNoteValues };
				}
			}
		}
	}
}

void KeyboardMacroView::saveSettings() {
	var result;
	
	for (auto macro : macros_) {
		var notes;
		for (auto note : macro.second.midiNotes) {
			notes.append(note);
		}
		auto def = new DynamicObject();
		def->setProperty("Notes", notes);
		def->setProperty("Event", String(KeyboardMacro::toText(macro.first)));
		result.append(def);
	}
	String json = JSON::toString(result);
	Settings::instance().set("MacroDefinitions", json.toStdString());	
	Settings::instance().flush();
}

void KeyboardMacroView::resized()
{
	auto area = getLocalBounds();

	// Needed width
	auto keyboardArea = area.removeFromTop(166);
	float keyboardDesiredWidth = keyboard_.getTotalKeyboardWidth() + 16;
	keyboard_.setBounds(keyboardArea.withSizeKeepingCentre((int) keyboardDesiredWidth, std::min(area.getHeight(), 150)).reduced(8));

	// Set up table
	for (auto c : configs_) {
		auto row = area.removeFromTop(40);
		c->setBounds(row.withSizeKeepingCentre(std::min(row.getWidth(), keyboard_.getWidth()), 30));
	}
}

bool KeyboardMacroView::isMacroState(KeyboardMacro const &macro)
{
	// Check if the keyboard state does contain all keys of the keyboard macro
	bool allDetected = true;
	for (int note : macro.midiNotes) {
		if (!state_.isNoteOnForChannels(0xffff, note)) {
			// No, this note is missing
			allDetected = false;
			break;
		}
	}
	// Check that no extra key is pressed
	bool extraKeyDetected = false;
	for (int note = 0; note < 128; note++) {
		if (state_.isNoteOnForChannels(0xffff, note)) {
			if (macro.midiNotes.find(note) == macro.midiNotes.end()) {
				extraKeyDetected = true;
				break;
			}
		}
	}
	return allDetected && !extraKeyDetected;
}
