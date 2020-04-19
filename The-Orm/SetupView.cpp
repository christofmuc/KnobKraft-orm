/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "SetupView.h"

#include "Synth.h"
#include "MidiChannelEntry.h"
#include "SoundExpanderCapability.h"
#include "Logger.h"
#include "AutoDetection.h"

#include "UIModel.h"

#include <boost/format.hpp>

SetupView::SetupView(midikraft::AutoDetection *autoDetection /*, HueLightControl *lights*/) :
	autoDetection_(autoDetection)/*, lights_(lights) */, 
	functionButtons_(1501, LambdaButtonStrip::Direction::Horizontal)
{
	// Build lists of input and output MIDI devices
	int i = 0;
	for (const auto &device : currentInputDevices()) {
		inputLookup_[++i] = device;
	}
	
	int j = 0;
	for (const auto &device : currentOutputDevices()) {
		outputLookup_[++j] = device;
	}

	for (auto &synth : UIModel::instance()->synthList_.allSynths()) {
		if (!synth.device()) continue;
		String sectionName = synth.device()->getName();
		
		// For each synth, we need 4 properties, and we need to listen to changes: 
		properties_.push_back(std::make_shared<TypedNamedValue>(TypedNamedValue({ "Activated", sectionName, Value(1), ValueType::Bool, 0, 1 })));
		properties_.back()->value.addListener(this);
		properties_.push_back(std::make_shared<TypedNamedValue>(TypedNamedValue({ "Sent to device", sectionName, Value(1), ValueType::Lookup, 1, (int) outputLookup_.size() + 1, outputLookup_ })));
		properties_.back()->value.addListener(this);
		properties_.push_back(std::make_shared<TypedNamedValue>(TypedNamedValue({ "Receive from device", sectionName, Value(1), ValueType::Lookup, 1, (int)inputLookup_.size() + 1, inputLookup_ })));
		properties_.back()->value.addListener(this);
		properties_.push_back(std::make_shared<TypedNamedValue>(TypedNamedValue({ "MIDI channel", sectionName, Value(1), ValueType::Integer, 1, 16})));
		properties_.back()->value.addListener(this);
	}
	refreshData();
	header_.setText("In case the auto-detection fails, setup the MIDI channel and MIDI interface below to get your synths detected.\n\n"
		"This can *not* be used to change the synth's channel, but rather in case the autodetection fails you can manually enter the correct channel here.");
	header_.setColour(TextEditor::ColourIds::outlineColourId, Colours::black);
	header_.setColour(TextEditor::ColourIds::backgroundColourId, Colours::black);
	header_.setMultiLine(true);
	header_.setReadOnly(true);
	addAndMakeVisible(header_);
	addAndMakeVisible(propertyEditor_);
	propertyEditor_.setProperties(properties_);

	// Define function buttons
	functionButtons_.setButtonDefinitions({
			{
			"synthDetection", {0, "Quick check", [this]() {
				quickConfigure();
			} } },
			{
			"autoconfigure",{1, "Rerun Autoconfigure", [this]() {
				auto currentSynths = UIModel::instance()->synthList_.activeSynths();
				autoDetection_->autoconfigure(currentSynths, nullptr);
				refreshData();
			} } } 
		});
	addAndMakeVisible(functionButtons_);

	UIModel::instance()->currentSynth_.addChangeListener(this);
}

SetupView::~SetupView() {
	UIModel::instance()->currentSynth_.removeChangeListener(this);
}

void SetupView::resized() {
	Rectangle<int> area(getLocalBounds());

	int width = std::min(area.getWidth(), 600);

	functionButtons_.setBounds(area.removeFromBottom(40).reduced(8));
	header_.setBounds(area.removeFromTop(100).withSizeKeepingCentre(width, 100).reduced(8));
	propertyEditor_.setBounds(area.withSizeKeepingCentre(width, area.getHeight()).reduced(8));
}

void SetupView::setValueWithoutListeners(Value &value, int newValue) {
	value.removeListener(this);
	value.setValue(newValue);
	value.addListener(this);
}

void SetupView::refreshData() {
	int prop = 0;
	for (auto &synth : UIModel::instance()->synthList_.allSynths()) {
		if (!synth.device()) continue;
		// Skip the active prop
		prop++;
		// Set output, input, and channel
		setValueWithoutListeners(properties_[prop++]->value, indexOfOutputDevice(synth.device()->midiOutput()));
		setValueWithoutListeners(properties_[prop++]->value, indexOfInputDevice(synth.device()->midiInput()));
		setValueWithoutListeners(properties_[prop++]->value, synth.device()->channel().toOneBasedInt());
	}
}

void SetupView::valueChanged(Value& value)
{
	// Determine the property that was changed
	for (auto prop : properties_) {
		if (prop->value.refersToSameSourceAs(value)) {
			std::shared_ptr<midikraft::SimpleDiscoverableDevice> synthFound;
			for (auto synth : UIModel::instance()->synthList_.allSynths()) {
				if (synth.device() && synth.device()->getName() == prop->sectionName) {
					synthFound = synth.device();
				}
			}

			if (synthFound) {
				if (prop->name == "Sent to device") {
					synthFound->setOutput(outputLookup_[value.getValue()]);
				}
				else if (prop->name == "Receive from device") {
					synthFound->setInput(inputLookup_[value.getValue()]);
				}
				else if (prop->name == "MIDI channel") {
					synthFound->setChannel(MidiChannel::fromOneBase(value.getValue()));
				}
				autoDetection_->persistSetting(synthFound.get());
			//	quickConfigure();
			}
			else {
				jassert(false);
			}
		}
	}
}

void SetupView::changeListenerCallback(ChangeBroadcaster* source)
{
	ignoreUnused(source);
	refreshData();
	/*// Find out which of the color selectors sent this message
	for (int i = 0; i < colours_.size(); i++) {
		if (colours_[i] == source) {
			auto newColour = colours_[i]->getCurrentColour();
			// Set the colour to our lights
			//lights_->setStudioLight(HueLightState(newColour), 0);
			// Persist the new colour in the synth
			//synths_[i].setColor(newColour);
		}
	}*/
}

void SetupView::quickConfigure()
{
	auto currentSynths = UIModel::instance()->synthList_.activeSynths();
	autoDetection_->quickconfigure(currentSynths); // This rather should be synchronous!
	refreshData();
}

std::vector<std::string> SetupView::currentOutputDevices() const  {
	std::vector<std::string> outputs;
	auto devices = MidiOutput::getDevices();
	for (const auto& device : devices) {
		outputs.push_back(device.toStdString());
	}
	return outputs;
}

//TODO this should go into MidiController
std::vector<std::string> SetupView::currentInputDevices() const {
	std::vector<std::string> inputs;
	auto devices = MidiInput::getDevices();
	for (const auto& device : devices) {
		inputs.push_back(device.toStdString());
	}
	return inputs;
}

int SetupView::indexOfOutputDevice(std::string const &outputDevice) const {
	for (auto const &d : outputLookup_) {
		if (d.second == outputDevice) {
			return d.first;
		}
	}
	return 0;
}

int SetupView::indexOfInputDevice(std::string const &inputDevice) const {
	for (auto const &d : inputLookup_) {
		if (d.second == inputDevice) {
			return d.first;
		}
	}
	return 0;
}


