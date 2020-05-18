/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "KeyboardMacroView.h"

#include "MasterkeyboardCapability.h"

#include "Logger.h"
#include "Settings.h"
#include "MidiChannelPropertyEditor.h"
#include "UIModel.h"

// Standardize text
const char *kMacrosEnabled = "Macros enabled";
const char *kAutomaticSetup = "Use current synth as master";
const char *kRouteMasterkeyboard = "Forward MIDI to synth";
const char *kInputDevice = "MIDI Input Device";
const char *kMidiChannel = "MIDI channel";
const char *kLowestNote = "Lowest MIDI Note";
const char *kHighestNote = "Highest MIDI Note";


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
	addAndMakeVisible(customSetup_);
	addAndMakeVisible(keyboard_);
	keyboard_.setOctaveForMiddleC(4); // This is correct for the DSI Synths, I just don't know what the standard is

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

		UIModel::instance()->currentSynth_.addChangeListener(this);
	}

	// Install keyboard handler to refresh midi keyboard display
	midikraft::MidiController::instance()->addMessageHandler(handle_, [this](MidiInput *source, MidiMessage const &message) {
		if (source && source->getName().toStdString() == customMasterkeyboardSetup_.typedNamedValueByName(kInputDevice)->lookupValue()) {
			if (customMasterkeyboardSetup_.valueByName(kRouteMasterkeyboard).getValue()) {
				// We want to route all events from the master keyboard to the synth of the current patch, so we can play it!
				auto currentPatch = UIModel::currentPatch();
				if (currentPatch.patch() && currentPatch.synth()) {
					auto location = dynamic_cast<midikraft::MidiLocationCapability *>(currentPatch.synth());
					if (location) {
						// Check if this is a channel message, and if yes, re-channel to the current synth
						MidiMessage channelMessage = message;
						if (message.getChannel() != 0) {
							channelMessage.setChannel(location->channel().toOneBasedInt());
						}
						midikraft::MidiController::instance()->getMidiOutput(location->midiOutput())->sendMessageNow(channelMessage);
					}
				}
			}
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
		}
	});

	// Load Macro Definitions
	setupPropertyEditor();
	loadFromSettings();
	refreshUI();
	setupKeyboardControl();
}

KeyboardMacroView::~KeyboardMacroView()
{
	midikraft::MidiController::instance()->removeMessageHandler(handle_);
	saveSettings();
}

void KeyboardMacroView::setupPropertyEditor() {
	customMasterkeyboardSetup_.clear();
	customMasterkeyboardSetup_.push_back(std::make_shared<TypedNamedValue>(kMacrosEnabled, "Setup", true));
	customMasterkeyboardSetup_.push_back(std::make_shared<TypedNamedValue>(kAutomaticSetup, "Setup", true));
	customMasterkeyboardSetup_.push_back(std::make_shared<TypedNamedValue>(kRouteMasterkeyboard, "MIDI Routing", true));
	customMasterkeyboardSetup_.push_back(std::make_shared<MidiDevicePropertyEditor>(kInputDevice, "Setup Masterkeyboard", true));
	customMasterkeyboardSetup_.push_back(std::make_shared<MidiChannelPropertyEditor>(kMidiChannel, "Setup Masterkeyboard"));
	customMasterkeyboardSetup_.push_back(std::make_shared<TypedNamedValue>(kLowestNote, "Setup Masterkeyboard", 0x24, 0, 127 ));
	customMasterkeyboardSetup_.push_back(std::make_shared<TypedNamedValue>(kHighestNote, "Setup Masterkeyboard", 0x60, 0, 127 ));
	for (auto tnv : customMasterkeyboardSetup_) {
		tnv->value().addListener(this);
	}
	customSetup_.setProperties(customMasterkeyboardSetup_);
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

	for (auto &prop : customMasterkeyboardSetup_) {
		std::string storedValue = Settings::instance().get(prop->name().toStdString());
		int intValue = std::atoi(storedValue.c_str());
		prop->value().setValue(intValue);
	}

	turnOnMasterkeyboardInput();
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

	for (auto &prop : customMasterkeyboardSetup_) {
		Settings::instance().set(prop->name().toStdString(), prop->value().toString().toStdString());
	}

	Settings::instance().flush();
}

void KeyboardMacroView::resized()
{
	auto area = getLocalBounds();

	// Needed width
	float keyboardDesiredWidth = keyboard_.getTotalKeyboardWidth() + 16;
	int contentWidth = std::min(area.getWidth(), 600);

	// On Top, the setup
	customSetup_.setBounds(area.removeFromTop(220).withSizeKeepingCentre(contentWidth, 220).reduced(8));
	// Then the keyboard	
	auto keyboardArea = area.removeFromTop(166);
	keyboard_.setBounds(keyboardArea.withSizeKeepingCentre((int)keyboardDesiredWidth, std::min(area.getHeight(), 150)).reduced(8));

	// Set up table
	for (auto c : configs_) {
		auto row = area.removeFromTop(40);
		c->setBounds(row.withSizeKeepingCentre(std::min(row.getWidth(), contentWidth), 30));
	}
}

void KeyboardMacroView::setupKeyboardControl() {
	int lowNote = customMasterkeyboardSetup_.valueByName(kLowestNote).getValue();
	int highNote = customMasterkeyboardSetup_.valueByName(kHighestNote).getValue();
	keyboard_.setAvailableRange(lowNote, highNote);
	keyboard_.setLowestVisibleKey(lowNote);
	resized();
}

void KeyboardMacroView::valueChanged(Value& value)
{
	if (value.refersToSameSourceAs(customMasterkeyboardSetup_.valueByName(kLowestNote)) ||
		value.refersToSameSourceAs(customMasterkeyboardSetup_.valueByName(kHighestNote))) {
		setupKeyboardControl();
	}
	else if (value.refersToSameSourceAs(customMasterkeyboardSetup_.valueByName(kInputDevice))) {
		turnOnMasterkeyboardInput();
	}
}

void KeyboardMacroView::turnOnMasterkeyboardInput() {
	if (customMasterkeyboardSetup_.valueByName(kRouteMasterkeyboard).getValue()) {
		String masterkeyboardDevice = customMasterkeyboardSetup_.typedNamedValueByName(kInputDevice)->lookupValue();
		midikraft::MidiController::instance()->enableMidiInput(masterkeyboardDevice.toStdString());
		SimpleLogger::instance()->postMessage("Opening master keyboard device " + masterkeyboardDevice + ", waiting for messages");
	}
}

void KeyboardMacroView::changeListenerCallback(ChangeBroadcaster* source)
{
	ignoreUnused(source);

	if (customMasterkeyboardSetup_.valueByName(kAutomaticSetup).getValue()) {
		// Mode 1 - follow current synth, use that as master keyboard
		auto currentSynth = UIModel::currentSynth();
		auto masterKeyboard = dynamic_cast<midikraft::MasterkeyboardCapability *>(currentSynth);
		auto location = dynamic_cast<midikraft::MidiLocationCapability *>(currentSynth);
		if (location) {
			auto tnv = customMasterkeyboardSetup_.typedNamedValueByName(kInputDevice);
			tnv->value().setValue(tnv->indexOfValue(location->midiInput()));
			tnv = customMasterkeyboardSetup_.typedNamedValueByName(kMidiChannel);
			auto midiChannel = std::dynamic_pointer_cast<MidiChannelPropertyEditor>(tnv);
			if (midiChannel) {
				if (masterKeyboard) {
					// If this is a real master keyboard (or a Yamaha RefaceDX), it might have a different output channel than input channel.
					midiChannel->setValue(masterKeyboard->getOutputChannel());
				}
				else {
					midiChannel->setValue(location->channel());
				}
			}
		}
		auto keyboard = dynamic_cast<midikraft::KeyboardCapability *>(currentSynth);
		if (keyboard) {
			customMasterkeyboardSetup_.valueByName(kLowestNote).setValue(keyboard->getLowestKey().noteNumber());
			customMasterkeyboardSetup_.valueByName(kHighestNote).setValue(keyboard->getHighestKey().noteNumber());
		}
	}
	else {
		// Automatic is off - don't change the current master keyboard. But the synth switch could mean that we were turned off during autodetection,
		// so turn back on again
		turnOnMasterkeyboardInput();
	}
}

bool KeyboardMacroView::isMacroState(KeyboardMacro const &macro)
{
	// Check if macros are turned on
	if (!customMasterkeyboardSetup_.valueByName(kMacrosEnabled).getValue()) {
		// No - then don't do anything
		return false;
	}

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
