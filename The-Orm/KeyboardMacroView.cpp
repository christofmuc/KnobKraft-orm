/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "KeyboardMacroView.h"

#include "MasterkeyboardCapability.h"

#include "Logger.h"
#include "Settings.h"
#include "UIModel.h"

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include "SpdLogJuce.h"

// Standardize text
const char *kMacrosEnabled = "Macros enabled";
const char *kAutomaticSetup = "Use current synth as master";
const char *kRouteMasterkeyboard = "Forward MIDI to synth";
const char *kFixedSynthSelected = "Fixed synth played";
const char *kUseElectraOne = "Forward Electra One";
const char *kInputDevice = "MIDI Input Device";
const char *kMidiChannel = "MIDI channel";
const char *kLowestNote = "Lowest MIDI Note";
const char *kHighestNote = "Highest MIDI Note";


class KeyboardMacroView::RecordProgress : private MidiKeyboardStateListener {
public:
	RecordProgress(Component* parent, MidiKeyboardState& state) : parent_(parent), state_(state), atLeastOneKey_(false)
	{
	}

	void show(std::function<void(std::set<int> const&, bool)> done) {
		done_ = std::move(done);
		auto options = juce::MessageBoxOptions().withButton("Clear").withButton("Cancel").withTitle("Press key(s) on your MIDI keyboard").withParentComponent(parent_);
		state_.addListener(this);
		messageBox_ = AlertWindow::showScopedAsync(options, [this](int button) {
			switch (button) {
			case 1:
				// Clear
				done_({}, false);
				break;
			case 0:
				// Cancel, nothing to do
				done_({}, true);
				break;
			default:
				spdlog::error("Unknown button number pressed, program error in RecordProgress of KeyboardMacroView");
			}
			});
	}

	virtual ~RecordProgress() override {
		state_.removeListener(this);
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
			messageBox_.close();
			done_(notes_, false);
		}
	}

private:
	Component* parent_;
	std::function<void(std::set<int> const&, bool)> done_;
	ScopedMessageBox messageBox_;
	std::set<int> notes_;
	MidiKeyboardState &state_;
	bool atLeastOneKey_;

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
				activeRecorder_ = std::make_shared<RecordProgress>(this, state_);
				activeRecorder_->show([this, event](std::set<int> const& notes, bool cancelled) {
					if (!cancelled) {
						KeyboardMacro newMacro = { event, notes };
						macros_[event] = newMacro;
						saveSettings();
						refreshUI();
					}
					activeRecorder_ = nullptr;
					}
				);
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
			int forwardMode = customMasterkeyboardSetup_.valueByName(kRouteMasterkeyboard).getValue();
			if (forwardMode == 2 || forwardMode == 3 || forwardMode == 4) {
				std::shared_ptr<midikraft::Synth> toWhichSynthToForward;
				if (forwardMode == 4) {
					// Fixed synth routing, that means, don't take away my master keyboard while I select a patch for a different synth
					auto selectedSynth = customMasterkeyboardSetup_.typedNamedValueByName(kFixedSynthSelected)->lookupValue();
					for (auto s : UIModel::instance()->synthList_.activeSynths()) {
						if (s->getName() == selectedSynth) {
							toWhichSynthToForward = std::dynamic_pointer_cast<midikraft::Synth>(s);
						}
					}
				}
				else if (forwardMode == 3) {
					// We want to route all events from the master keyboard to the synth of the current patch, so we can play it!
					// Forward to the synth of the current patch
					auto currentPatch = UIModel::currentPatch();
					if (currentPatch.patch()) {
						toWhichSynthToForward = currentPatch.smartSynth();
					}
				}
				if (forwardMode == 2 || !toWhichSynthToForward) {
					// Forward to the synth selected in the top row
					toWhichSynthToForward = UIModel::instance()->currentSynth_.smartSynth();
				}
				if (toWhichSynthToForward) {
					auto location = midikraft::Capability::hasCapability<midikraft::MidiLocationCapability>(toWhichSynthToForward);
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

				// Macros only fire on key down
				if (message.isNoteOn()) {
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
	// Midi Device Selector can broadcast a change message when a new device is detected or removed
	midiDeviceList_ = std::make_shared<MidiDevicePropertyEditor>(kInputDevice, "Setup Masterkeyboard", true);
	midikraft::MidiController::instance()->addChangeListener(this);

	customMasterkeyboardSetup_.clear();
	customMasterkeyboardSetup_.push_back(std::make_shared<TypedNamedValue>(kMacrosEnabled, "Setup", true));
	customMasterkeyboardSetup_.push_back(std::make_shared<TypedNamedValue>(kAutomaticSetup, "Setup", true));
	std::map<int, std::string> lookup = { {1, "No forwarding"}, {2, "Forward to selected synth"}, { 3, "Forward to synth of current patch "}, {4, "Always forward to the fixed synth set below" } };
	customMasterkeyboardSetup_.push_back(std::make_shared<TypedNamedValue>(kRouteMasterkeyboard, "MIDI Routing", 1, lookup));
	UIModel::instance()->synthList_.addChangeListener(this);
	synthListEditor_ = std::make_shared<TypedNamedValue>(kFixedSynthSelected, "MIDI Routing", 1, std::map<int, std::string>());
	refreshSynthList();
	customMasterkeyboardSetup_.push_back(synthListEditor_);
	customMasterkeyboardSetup_.push_back(std::make_shared<TypedNamedValue>(kUseElectraOne, "MIDI Routing", false));
	customMasterkeyboardSetup_.push_back(midiDeviceList_);
	customMasterkeyboardSetup_.push_back(std::make_shared<MidiChannelPropertyEditor>(kMidiChannel, "Setup Masterkeyboard"));
	customMasterkeyboardSetup_.push_back(std::make_shared<TypedNamedValue>(kLowestNote, "Setup Masterkeyboard", 0x24, 0, 127));
	customMasterkeyboardSetup_.push_back(std::make_shared<TypedNamedValue>(kHighestNote, "Setup Masterkeyboard", 0x60, 0, 127));
	for (auto tnv : customMasterkeyboardSetup_) {
		tnv->value().addListener(this);
	}
	customSetup_.setProperties(customMasterkeyboardSetup_);
}

void KeyboardMacroView::refreshSynthList()
{
	std::map<int, std::string> synth_list;
	int i = 1;
	for (auto s : UIModel::instance()->synthList_.activeSynths()) {
		synth_list.emplace(i++, s->getName());
	}
	synthListEditor_->setLookup(synth_list);
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
	if (!json.empty()) {
		try {
			auto macros = nlohmann::json::parse(json);
			if (macros.is_array()) {
				for (auto macro : macros) {
					if (macro.is_object()) {
						std::set<int> midiNoteValues;
						auto notes = macro["Notes"];
						if (notes.is_array()) {
							for (auto noteVar : notes) {
								if (noteVar.is_number_integer()) {
									midiNoteValues.insert((int)noteVar);
								}
							}
						}
						auto event = macro["Event"];
						KeyboardMacroEvent macroEventCode = KeyboardMacroEvent::Unknown;
						if (event.is_string()) {
							std::string eventString = event;
							macroEventCode = KeyboardMacro::fromText(eventString);
						}
						if (macroEventCode != KeyboardMacroEvent::Unknown && !midiNoteValues.empty()) {
							macros_[macroEventCode] = { macroEventCode, midiNoteValues };
						}
					}
				}
			}

			for (auto& prop : customMasterkeyboardSetup_) {
				std::string storedValue = Settings::instance().get(prop->name().toStdString());
				int intValue = std::atoi(storedValue.c_str());
				prop->value().setValue(intValue);
			}
		}
		catch (nlohmann::json::parse_error& e) {
			spdlog::error("Keyboard macro definition corrupt in settings file, not loading. Error is {}", e.what());
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
	customSetup_.setBounds(area.removeFromTop(260).withSizeKeepingCentre(contentWidth, 260).reduced(8));
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
	else if (value.refersToSameSourceAs(customMasterkeyboardSetup_.valueByName(kUseElectraOne))) {
		controllerRouter_.enable(value.getValue());
	}
}

void KeyboardMacroView::turnOnMasterkeyboardInput() {
	String masterkeyboardDevice = customMasterkeyboardSetup_.typedNamedValueByName(kInputDevice)->lookupValue();
	if (masterkeyboardDevice.isNotEmpty()) {
        auto device = midikraft::MidiController::instance()->getMidiInputByName(masterkeyboardDevice.toStdString());
		midikraft::MidiController::instance()->enableMidiInput(device);
		spdlog::info("Opening master keyboard device {}, waiting for messages", masterkeyboardDevice);
	}
}

void KeyboardMacroView::changeListenerCallback(ChangeBroadcaster* source)
{
	if (source == midikraft::MidiController::instance()) {
		// The list of MIDI devices changed, need to refresh the property editor
		midiDeviceList_->refreshDeviceList();
		customSetup_.setProperties(customMasterkeyboardSetup_);
	}
	else if (source == &UIModel::instance()->synthList_) {
		refreshSynthList();
	}
	else if (customMasterkeyboardSetup_.valueByName(kAutomaticSetup).getValue()) {
		// Mode 1 - follow current synth, use that as master keyboard
		auto currentSynth = UIModel::instance()->currentSynth_.smartSynth();
		auto masterKeyboard = midikraft::Capability::hasCapability<midikraft::MasterkeyboardCapability>(currentSynth);
		auto location = midikraft::Capability::hasCapability<midikraft::MidiLocationCapability>(currentSynth);
		if (location) {
			auto tnv = customMasterkeyboardSetup_.typedNamedValueByName(kInputDevice);
			tnv->value().setValue(tnv->indexOfValue(location->midiInput().name.toStdString()));
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
		auto keyboard = midikraft::Capability::hasCapability<midikraft::KeyboardCapability>(currentSynth);
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
