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
#include "GenericAdaptation.h"
#include "CreateNewAdaptationDialog.h"
#include "AutoDetectProgressWindow.h"
#include "MidiLoopbackTest.h"


#include "UIModel.h"

#include "ColourHelpers.h"

#include <spdlog/spdlog.h>
#include "SpdLogJuce.h"
#include <algorithm>

class MidiChannelPropertyEditorWithOldDevices : public MidiDevicePropertyEditor {
public:
	MidiChannelPropertyEditorWithOldDevices(std::string const &title, std::string const &sectionName, bool inputInsteadOutput) : MidiDevicePropertyEditor(title, sectionName, inputInsteadOutput) {
		if (inputInsteadOutput) {
			auto set = midikraft::MidiController::instance()->currentInputs(true);
			juce::Array<juce::MidiDeviceInfo> list;
            for (auto device : set)
            {
                list.add(device);
            }
			refreshDropdownList(list);
		}
		else {
			auto set = midikraft::MidiController::instance()->currentOutputs(true);
            juce::Array<juce::MidiDeviceInfo> list;
            for (auto device : set)
            {
                list.add(device);
            }
			refreshDropdownList(list);
		}
	}
};


const char *kSetupHint1 = "In case the auto-detection fails, setup the MIDI channel and MIDI interface below to get your synths detected.\n\n"
	"This can *not* be used to change the synth's channel, but rather in case the autodetection fails you can manually enter the correct channel here.";
const char *kSetupHint2 = "First please select at least one synth to use, then turn it on and press auto-detect to detect if a working bi-directional connection can be made.\n\n";


SetupView::SetupView(midikraft::AutoDetection *autoDetection /*, HueLightControl *lights*/) :
	autoDetection_(autoDetection)/*, lights_(lights) */
{
	// We have two lists: One is the list of synths, where you just activate and deactivate them, and the second is the detail list which shows the
	// individual synths setup
	std::vector<std::string> sortedList;
	for (auto &synth : UIModel::instance()->synthList_.allSynths()) {
		if (!synth.device()) continue;
		sortedList.push_back(synth.getName());
	}
	std::sort(sortedList.begin(), sortedList.end());

	for (auto &synth : sortedList) {
		sortedSynthList_.push_back(UIModel::instance()->synthList_.synthByName(synth));
		synths_.push_back(std::make_shared<TypedNamedValue>(sortedSynthList_.back().getName(), "Activate support for synth", true));
	}

	// We need to know if any of these are clicked
	for (auto prop : synths_) prop->value().addListener(this);
	rebuildSetupColumn();
	refreshSynthActiveness();
	addAndMakeVisible(header_);
	addAndMakeVisible(synthSelection_);
	synthSelection_.setProperties(synths_);
	addAndMakeVisible(synthSetup_);
	synthSetup_.setProperties(properties_);

	// I want one very prominent button for auto-configure, because that should normally the first one to press
	addAndMakeVisible(autoConfigureButton_);
	autoConfigureButton_.onClick = [this]() {
		autoDetect();
	};
	autoConfigureButton_.setButtonText("Auto-Detect");

	midikraft::MidiController::instance()->addChangeListener(this);

	UIModel::instance()->currentSynth_.addChangeListener(this);
}

SetupView::~SetupView() {
	UIModel::instance()->currentSynth_.removeChangeListener(this);
}

void SetupView::resized() {
	Rectangle<int> area(getLocalBounds());

	int width = std::min(area.getWidth(), 600);
	header_.setBounds(area.removeFromTop(100).withSizeKeepingCentre(width, 100).reduced(8));

	// Two column setup, don't go to wide, I don't need more than 1000 pixels
	int setupWidth = std::min(area.getWidth(), 1000);
	synthSelection_.setBounds(area.removeFromLeft(area.getWidth() / 2).removeFromRight(setupWidth/2).reduced(8));
	auto rightColumn = area.removeFromLeft(setupWidth / 2);
	autoConfigureButton_.setBounds(rightColumn.removeFromTop(40).withSizeKeepingCentre(120, 30));
	synthSetup_.setBounds(rightColumn);
}

void SetupView::setValueWithoutListeners(Value &value, int newValue) {
	value.removeListener(this);
	value.setValue(newValue);
	value.addListener(this);
}

void SetupView::rebuildSetupColumn() {
	// Cleanup
	for (auto prop : properties_) prop->value().removeListener(this);
	properties_.clear();

	// Rebuild
	for (auto &synth: sortedSynthList_) {
		if (!UIModel::instance()->synthList_.isSynthActive(synth.device())) continue;
		auto sectionName = synth.getName();
		// For each synth, we need 3 properties, and we need to listen to changes: 
		properties_.push_back(std::make_shared<MidiChannelPropertyEditorWithOldDevices>("Sent to device", sectionName, false));
		properties_.push_back(std::make_shared<MidiChannelPropertyEditorWithOldDevices>("Receive from device", sectionName, true));
		properties_.push_back(std::make_shared<MidiChannelPropertyEditor>("MIDI channel", sectionName));
	}
	// We need to know if any of these are clicked
	for (auto prop : properties_) prop->value().addListener(this);

	synthSetup_.setProperties(properties_);
	refreshData();

	// Display a helpful text
	header_.setText(properties_.empty() ? kSetupHint2 : kSetupHint1);
}

void SetupView::refreshSynthActiveness() {
	int synthCount = 0;
	for (auto &synth : sortedSynthList_) {
		// Skip the active prop
		setValueWithoutListeners(synths_[synthCount++]->value(), UIModel::instance()->synthList_.isSynthActive(synth.device()));
	}
}

void SetupView::refreshData() {
	size_t prop = 0;
	
	for (auto &synth : sortedSynthList_) {
		if (!synth.device()) continue;
		if (!UIModel::instance()->synthList_.isSynthActive(synth.device())) continue;
		// Load
		midikraft::AutoDetection::loadSettings(synth.device().get());
		// Set output, input, and channel
		setValueWithoutListeners(properties_[prop]->value(), properties_[prop]->findOrAppendLookup(synth.device()->midiOutput().name.toStdString()));
		prop++;
		setValueWithoutListeners(properties_[prop]->value(), properties_[prop]->findOrAppendLookup(synth.device()->midiInput().name.toStdString()));
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
	// Determine the property that was changed, first search in the synth activation properties, and then in the synth setup properties
	for (auto prop : synths_) {
		if (prop->value().refersToSameSourceAs(value)) {
			auto synthFound = UIModel::instance()->synthList_.synthByName(prop->name().toStdString());
			if (synthFound.device()) {
				UIModel::instance()->synthList_.setSynthActive(synthFound.device().get(), value.getValue());
				auto activeKey = String(synthFound.getName()) + String("-activated");
				Settings::instance().set(activeKey.toStdString(), value.getValue().toString().toStdString());
				autoDetection_->persistSetting(synthFound.device().get());
				rebuildSetupColumn();
				return;
			}
			else {
				jassertfalse;
			}
		}
	}
	for (auto prop : properties_) {
		if (prop->value().refersToSameSourceAs(value)) {
			auto synthFound = UIModel::instance()->synthList_.synthByName(prop->sectionName().toStdString());
			if (synthFound.device()) {
				if (prop->name() == "Sent to device") {
                    auto outputName = prop->lookup()[value.getValue()];
                    synthFound.device()->setOutput(midikraft::MidiController::instance()->getMidiOutputByName(outputName));
				}
				else if (prop->name() == "Receive from device") {
                    auto inputName = prop->lookup()[value.getValue()];
					synthFound.device()->setInput(midikraft::MidiController::instance()->getMidiInputByName(inputName));
				}
				else if (prop->name() == "MIDI channel") {
					synthFound.device()->setChannel(MidiChannel::fromOneBase(value.getValue()));
				}
				else if (prop->name() == "Activated") {
					UIModel::instance()->synthList_.setSynthActive(synthFound.device().get(), value.getValue());
					auto activeKey = String(synthFound.getName()) + String("-activated");
					Settings::instance().set(activeKey.toStdString(), value.getValue().toString().toStdString());
				}
				else {
					// New property? Implement handler here
					jassertfalse;
				}
				autoDetection_->persistSetting(synthFound.device().get());
				/*timedAction_.callDebounced([this]() {
					quickConfigure();
				}, 1000);*/
				return;
			}
			else {
				jassertfalse;
			}
		}
	}
}

void SetupView::changeListenerCallback(ChangeBroadcaster* source)
{
	if (source == midikraft::MidiController::instance()) {
		// Refresh setup list on the right side
		rebuildSetupColumn();
	}
	else {
		// Refresh left side
		refreshSynthActiveness();
		refreshData();
	}
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

void SetupView::createNewAdaptation()
{
	knobkraft::CreateNewAdaptationDialog::showDialog(&synthSetup_);
}

class MidiTestProgressWindow : public ProgressHandlerWindow, public std::enable_shared_from_this<MidiTestProgressWindow> {
public:
	MidiTestProgressWindow(juce::MidiDeviceInfo input, juce::MidiDeviceInfo output) :
		ProgressHandlerWindow("Testing MIDI connection...", "Preparing MIDI loopback test..."),
		input_(std::move(input)), output_(std::move(output)) {
	}

	virtual void run() override {
		report = midikraft::MidiLoopbackTest::run(input_, output_, shared_from_this());
	}

	midikraft::MidiLoopbackTestReport report;

private:
	juce::MidiDeviceInfo input_;
	juce::MidiDeviceInfo output_;
};

namespace {

	std::string statusName(midikraft::MidiLoopbackTestStatus status)
	{
		switch (status) {
		case midikraft::MidiLoopbackTestStatus::Passed: return "PASS";
		case midikraft::MidiLoopbackTestStatus::TimedOut: return "TIMEOUT";
		case midikraft::MidiLoopbackTestStatus::Incomplete: return "INCOMPLETE";
		case midikraft::MidiLoopbackTestStatus::Mismatch: return "MISMATCH";
		case midikraft::MidiLoopbackTestStatus::Cancelled: return "CANCELLED";
		}
		return "UNKNOWN";
	}

	juce::String resultText(midikraft::MidiLoopbackTestReport const& report)
	{
		if (!report.error.empty()) return report.error;

		juce::String text;
		text << "Output: " << report.midiOutput.name << "\n"
			<< "Input: " << report.midiInput.name << "\n\n";
		for (auto const& result : report.results) {
			text << result.name << ": " << statusName(result.status)
				<< " (" << result.received.size() << "/" << result.expected.size()
				<< " bytes, " << result.elapsedMilliseconds << " ms)";
			if (result.firstDifferentByte >= 0) {
				text << ", first difference at offset " << result.firstDifferentByte;
			}
			text << "\n";
		}
		if (report.cancelled) text << "\nTest cancelled.";
		return text;
	}

}

void SetupView::midiTest()
{
	auto inputs = juce::MidiInput::getAvailableDevices();
	auto outputs = juce::MidiOutput::getAvailableDevices();
	if (inputs.isEmpty() || outputs.isEmpty()) {
		juce::AlertWindow::showMessageBox(juce::AlertWindow::WarningIcon, "MIDI Test",
			"At least one MIDI input and one MIDI output are required.");
		return;
	}

	juce::StringArray inputNames;
	for (auto const& input : inputs) inputNames.add(input.name);
	juce::StringArray outputNames;
	for (auto const& output : outputs) outputNames.add(output.name);

	juce::AlertWindow selector("MIDI Test",
		"Connect the selected MIDI output directly to the selected MIDI input with a cable. "
		"The test sends channel messages and deterministic SysEx data up to 16 KiB, then compares what returns byte-for-byte.",
		juce::AlertWindow::QuestionIcon);
	selector.addComboBox("midiOutput", outputNames, "MIDI output");
	selector.addComboBox("midiInput", inputNames, "MIDI input");
	selector.getComboBoxComponent("midiOutput")->setSelectedItemIndex(0);
	selector.getComboBoxComponent("midiInput")->setSelectedItemIndex(0);
	selector.addButton("Run test", 1, juce::KeyPress(juce::KeyPress::returnKey));
	selector.addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
	if (selector.runModalLoop() != 1) return;

	const auto outputIndex = selector.getComboBoxComponent("midiOutput")->getSelectedItemIndex();
	const auto inputIndex = selector.getComboBoxComponent("midiInput")->getSelectedItemIndex();
	if (!juce::isPositiveAndBelow(outputIndex, outputs.size()) || !juce::isPositiveAndBelow(inputIndex, inputs.size())) return;

	auto progressWindow = std::make_shared<MidiTestProgressWindow>(inputs.getReference(inputIndex), outputs.getReference(outputIndex));
	progressWindow->runThread();

	const auto text = resultText(progressWindow->report);
	if (progressWindow->report.allPassed()) {
		spdlog::info("MIDI loopback test passed:\n{}", text.toStdString());
		juce::AlertWindow::showMessageBox(juce::AlertWindow::InfoIcon, "MIDI Test passed", text);
	}
	else {
		spdlog::warn("MIDI loopback test did not pass:\n{}", text.toStdString());
		juce::AlertWindow::showMessageBox(juce::AlertWindow::WarningIcon, "MIDI Test results", text);
	}
}

void SetupView::autoDetect() {
	auto currentSynths = UIModel::instance()->synthList_.activeSynths();
	AutoDetectProgressWindow window(currentSynths);
	if (window.runThread()) {
		refreshData();
	}
}
