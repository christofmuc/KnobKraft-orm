/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "SetupView.h"

#include "Synth.h"
#include "MidiChannelPropertyEditor.h"
#include "SoundExpanderCapability.h"
#include "Logger.h"
#include "AutoDetection.h"
#include "Settings.h"
#include "GenericAdaption.h"
#include "CreateNewAdaptionDialog.h"

#include "UIModel.h"

#include "ColourHelpers.h"

#include <boost/format.hpp>

SetupView::SetupView(midikraft::AutoDetection *autoDetection /*, HueLightControl *lights*/) :
	autoDetection_(autoDetection)/*, lights_(lights) */,
	functionButtons_(1501, LambdaButtonStrip::Direction::Horizontal)
{
	for (auto &synth : UIModel::instance()->synthList_.allSynths()) {
		if (!synth.device()) continue;
		auto sectionName = synth.device()->getName();

		// For each synth, we need 4 properties, and we need to listen to changes: 
		properties_.push_back(std::make_shared<TypedNamedValue>("Activated", sectionName, true));
		properties_.push_back(std::make_shared<MidiDevicePropertyEditor>("Sent to device", sectionName, false));
		properties_.push_back(std::make_shared<MidiDevicePropertyEditor>("Receive from device", sectionName, true));
		properties_.push_back(std::make_shared<MidiChannelPropertyEditor>("MIDI channel", sectionName));
		for (auto prop : properties_) prop->value().addListener(this);
	}
	refreshData();
	header_.setMultiLine(true);
	header_.setReadOnly(true);
	header_.setColour(TextEditor::ColourIds::outlineColourId, ColourHelpers::getUIColour(&header_, LookAndFeel_V4::ColourScheme::windowBackground));
	header_.setColour(TextEditor::ColourIds::backgroundColourId, ColourHelpers::getUIColour(&header_, LookAndFeel_V4::ColourScheme::windowBackground));
	header_.setText("In case the auto-detection fails, setup the MIDI channel and MIDI interface below to get your synths detected.\n\n"
		"This can *not* be used to change the synth's channel, but rather in case the autodetection fails you can manually enter the correct channel here.");
	addAndMakeVisible(header_);
	addAndMakeVisible(propertyEditor_);
	propertyEditor_.setProperties(properties_);

	// Define function buttons
	functionButtons_.setButtonDefinitions({
			{
			"synthDetection", {0, "Re-check connectivity", [this]() {
				quickConfigure();
			} } },
			{"selectAdaptionDirectory", {1, "Set User Adaption Dir", [this]() {
				FileChooser directoryChooser("Please select the directory to store your user adaptions...", File(knobkraft::GenericAdaption::getAdaptionDirectory()));
				if (directoryChooser.browseForDirectory()) {
					knobkraft::GenericAdaption::setAdaptionDirectoy(directoryChooser.getResult().getFullPathName().toStdString());
					juce::AlertWindow::showMessageBox(AlertWindow::InfoIcon, "Restart required", "Your new adaptions directory will only be used after a restart of the application!");
				}
			} } },
			{"createNewAdaption", {2, "Create new adaption", [this]() {
				knobkraft::CreateNewAdaptionDialog::showDialog(&propertyEditor_);
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
		setValueWithoutListeners(properties_[prop++]->value(), UIModel::instance()->synthList_.isSynthActive(synth.device()));
		// Set output, input, and channel
		setValueWithoutListeners(properties_[prop]->value(), properties_[prop]->indexOfValue(synth.device()->midiOutput()));
		prop++;
		setValueWithoutListeners(properties_[prop]->value(), properties_[prop]->indexOfValue(synth.device()->midiInput()));
		prop++;
		if (!synth.device()->channel().isValid()) {
			setValueWithoutListeners(properties_[prop++]->value(), 18);
		}
		else if (synth.device()->channel().isOmni()) {
			setValueWithoutListeners(properties_[prop++]->value(), 17);
		}
		else {
			setValueWithoutListeners(properties_[prop++]->value(), synth.device()->channel().toOneBasedInt());
		}
	}
}

void SetupView::valueChanged(Value& value)
{
	// Determine the property that was changed
	for (auto prop : properties_) {
		if (prop->value().refersToSameSourceAs(value)) {
			std::shared_ptr<midikraft::SimpleDiscoverableDevice> synthFound;
			for (auto synth : UIModel::instance()->synthList_.allSynths()) {
				if (synth.device() && synth.device()->getName() == prop->sectionName()) {
					synthFound = synth.device();
				}
			}

			if (synthFound) {
				if (prop->name() == "Sent to device") {
					synthFound->setOutput(prop->lookup()[value.getValue()]);
				}
				else if (prop->name() == "Receive from device") {
					synthFound->setInput(prop->lookup()[value.getValue()]);
				}
				else if (prop->name() == "MIDI channel") {
					synthFound->setChannel(MidiChannel::fromOneBase(value.getValue()));
				}
				else if (prop->name() == "Activated") {
					UIModel::instance()->synthList_.setSynthActive(synthFound.get(), value.getValue());
					auto activeKey = String(synthFound->getName()) + String("-activated");
					Settings::instance().set(activeKey.toStdString(), value.getValue().toString().toStdString());
				}
				autoDetection_->persistSetting(synthFound.get());
				/*timedAction_.callDebounced([this]() {
					quickConfigure();
				}, 1000);*/
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


