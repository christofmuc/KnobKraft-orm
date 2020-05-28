/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "BCR2000_Component.h"

#include "RotaryWithLabel.h"

#include "MidiController.h"
#include "Logger.h"
#include "Synth.h"
#include "UIModel.h"
#include "SupportedByBCR2000.h"
#include "SynthParameterDefinition.h"
#include "Settings.h"
#include "Sysex.h"
#include "DetailedParametersCapability.h"
#include "BidirectionalSyncCapability.h"
#include "SendsProgramChangeCapability.h"

#include "MidiHelpers.h"

const char *kLastPathBCL = "lastPathBCL";

// See https://www.sequencer.de/synth/index.php/B-Control-Tokenreferenz for the button layout info
std::map<int, std::string> defaultLabels = {
	{ 57, "Group 1"}, { 58, "Group 2"}, { 59, "Group 3"}, { 60, "Group 4"},
	{ 53, "STORE"}, { 54, "LEARN"}, { 55, "EDIT"}, { 56, "EXIT"}, 
	{ 63, "Preset <"}, { 64, "Preset >"},
};

BCR2000_Component::BCR2000_Component(std::shared_ptr<midikraft::BCR2000> bcr) : bcr2000_(bcr), updateSynthListener_(this), updateControllerListener_(this), librarian_({})
{
	// Create 7*8 rotary knobs for the BCR2000 display
	for (int i = 0; i < 7 * 8; i++) {
		RotaryWithLabel *knob;
		if (i < 32) {
			knob = new RotaryWithLabelAndButtonFunction();
		}
		else {
			knob = new RotaryWithLabel();
		}
		rotaryKnobs.add(knob);
		addAndMakeVisible(*knob);
	}
	// Create 64 "press knobs", that includes those buttons that are on the encoders and those on the right side of the BCR2000
	for (int i = 0; i < 64; i++) {
		auto press = new ToggleButton();
		if (defaultLabels.find(i + 1) != defaultLabels.end()) press->setButtonText(defaultLabels[i + 1]);
		pressKnobs.add(press);
		addAndMakeVisible(press);
	}

	// Extra function buttons
	LambdaButtonStrip::TButtonMap buttons = {
	{ "loadBCR",{ 0, "Debug: Load BCL file from computer into BCR2000", [this]() {
		std::string lastPath = Settings::instance().get(kLastPathBCL, File::getSpecialLocation(File::userDocumentsDirectory).getFullPathName().toStdString());
		FileChooser bclChooser("Please select the BCL file you want to send to the BCR2000...", File(lastPath), "*.txt;*.bcr;*.bcl");
		if (bclChooser.browseForFileToOpen())
		{
			File bclFile(bclChooser.getResult());
			Settings::instance().set(kLastPathBCL, bclFile.getParentDirectory().getFullPathName().toStdString());
			String content = bclFile.loadFileAsString();
			midikraft::MidiController::instance()->enableMidiInput(bcr2000_->midiInput()); // Make sure we listen to the answers from the BCR2000 that we detected!
			bcr2000_->sendSysExToBCR(midikraft::MidiController::instance()->getMidiOutput(bcr2000_->midiOutput()), bcr2000_->convertToSyx(content.toStdString()), [](std::vector<midikraft::BCR2000::BCRError> const &errors) {
				ignoreUnused(errors); //TODO
			});
			bcr2000_->invalidateListOfPresets();
		}
	} } },
	{ "loadSYX",{ 1, "Debug: Load sysex file from computer into BCR2000", [this]() {
		std::string lastPath = Settings::instance().get(kLastPathBCL, File::getSpecialLocation(File::userDocumentsDirectory).getFullPathName().toStdString());
		FileChooser syxChooser("Please select the sysex you want to send to the BCR2000...", File(lastPath), "*.syx");
		if (syxChooser.browseForFileToOpen())
		{
			File syxFile(syxChooser.getResult());
			Settings::instance().set(kLastPathBCL, syxFile.getParentDirectory().getFullPathName().toStdString());
			std::vector<MidiMessage> sysex = Sysex::loadSysex(syxChooser.getResult().getFullPathName().toStdString());
			midikraft::MidiController::instance()->enableMidiInput(bcr2000_->midiInput()); // Make sure we listen to the answers from the BCR2000 that we detected!
			bcr2000_->sendSysExToBCR(midikraft::MidiController::instance()->getMidiOutput(bcr2000_->midiOutput()), sysex, [](std::vector<midikraft::BCR2000::BCRError> const &errors) {
				ignoreUnused(errors); //TODO
			});
			bcr2000_->invalidateListOfPresets();
		}
	} } },
	};
	buttons_ = std::make_unique<LambdaButtonStrip>(505, LambdaButtonStrip::Direction::Horizontal);
	buttons_->setButtonDefinitions(buttons);
	addAndMakeVisible(*buttons_);

	// Finally make sure we get notified if the current synth changes
	UIModel::instance()->currentSynth_.addChangeListener(this);
	UIModel::instance()->currentPatch_.addChangeListener(this);
	UIModel::instance()->currentPatchValues_.addChangeListener(this);
}

BCR2000_Component::~BCR2000_Component()
{
	UIModel::instance()->currentPatchValues_.removeChangeListener(this);
	UIModel::instance()->currentPatch_.removeChangeListener(this);
	UIModel::instance()->currentSynth_.removeChangeListener(this);
}

void BCR2000_Component::changeListenerCallback(ChangeBroadcaster* source) {
	CurrentSynth *current = dynamic_cast<CurrentSynth *>(source);
	if (current) {
		for (auto knob : rotaryKnobs) {
			knob->setUnused();
		}
		auto supported = dynamic_cast<midikraft::SupportedByBCR2000 *>(current->synth());
		if (supported) {
			// Code to setup the BCR2000
			auto setupBCR = [this, supported]() {
			/*	bool allFound = true;
				int firstIndex = -1;
				for (auto preset : supported->presetNames()) {
					// Check if we already have the preset stored somewhere in the BCR
					int index = bcr2000_.indexOfPreset(preset);
					if (index == -1) {
						allFound = false;
					}
					else {
						if (firstIndex == -1) {
							firstIndex = index;
						}
					}
				}
				if (allFound) {
					// All of them are already there, all we need to do is to select the first found index!
					bcr2000_.selectPreset(midikraft::MidiController::instance(), firstIndex);
				}
				else*/ {
					// At least one is missing, resend the whole stuff
					supported->setupBCR2000(*bcr2000_);
					bcr2000_->invalidateListOfPresets();
				}
			};
			bcr2000_->refreshListOfPresets(setupBCR);

			// The view we can always setup already
			uiModel_ = createParameterModel();
			// Add all Values of the uiModel_ into a ValueTree
			uiValueTree_ = ValueTree("UIMODEL");
			for (auto param : uiModel_) {
				uiValueTree_.setProperty(param->name(), param->value().getValue(), nullptr);
				auto v = uiValueTree_.getPropertyAsValue(Identifier(param->name()), nullptr, false);
				param->value().referTo(v);
			}

			supported->setupBCR2000View(this, uiModel_, uiValueTree_);

			// Now attach a sysex generating listener to the values of the ValueTree
			uiValueTree_.addListener(&updateSynthListener_);
			uiValueTree_.addListener(&updateControllerListener_);
		}
	}
	else if (dynamic_cast<CurrentPatch *>(source) || source == &UIModel::instance()->currentPatchValues_) {
		updateSynthListener_.updateAllKnobsFromPatch(UIModel::currentPatch().patch());
	}
}

TypedNamedValueSet BCR2000_Component::createParameterModel() {
	TypedNamedValueSet result;

	auto detailedParameters = dynamic_cast<midikraft::DetailedParametersCapability*>(UIModel::currentSynthOfPatch());
	if (detailedParameters) {
		for (auto param : detailedParameters->allParameterDefinitions()) {
			auto intParam = std::dynamic_pointer_cast<midikraft::SynthIntParameterCapability>(param);
			if (intParam) {
				switch (param->type()) {
				case midikraft::SynthParameterDefinition::ParamType::INT:
					result.push_back(std::make_shared<TypedNamedValue>(param->name(), "KawaiK3", 0, intParam->minValue(), intParam->maxValue()));
					break;
				case midikraft::SynthParameterDefinition::ParamType::LOOKUP: {
					std::map<int, std::string> lookup;
					auto lookupCap = std::dynamic_pointer_cast<midikraft::SynthLookupParameterCapability>(param);
					for (int i = intParam->minValue(); i < intParam->maxValue(); i++) {
						lookup.emplace(i, lookupCap->valueAsText(i));
					}
					result.push_back(std::make_shared<TypedNamedValue>(param->name(), "KawaiK3", 0, lookup));
					break;
				}
				default:
					jassertfalse;
				}
			}
			else {
				jassertfalse;
			}
		}
	}
	return result;
}

void BCR2000_Component::resized()
{
	Rectangle<int> area(getLocalBounds());

	// Button strip
	buttons_->setBounds(area.removeFromBottom(60).reduced(10));

	// 9*8 layout
	Grid grid;
	using Track = Grid::TrackInfo;
	for (int i = 0; i < 9; i++) grid.templateRows.add(Track(1_fr));
	for (int i = 0; i < 10; i++) grid.templateColumns.add(Track(1_fr));
	// Funny mixed layout matching the hardware BCR2000
	int knob = 0; 
	int press = 0;
	int groupButtons = 56;
	int storeButtons = 52;
	int presetButtons = 62;
	int lowerButtons = 48;
	for (int row = 0; row < 9; row++) {
		for (int col = 0; col < 10; col++) {
			if (col < 8) {
				if (row < 4 || row > 5)
					grid.items.add(GridItem(rotaryKnobs[knob++]));
				else
					grid.items.add(GridItem(pressKnobs[press++]));
			}
			else {
				switch (row) {
				case 0:
				case 1:
					grid.items.add(GridItem(pressKnobs[groupButtons++]));
					break;
				case 2:
				case 3: {
					Label label; //TODO Is this safe=
					grid.items.add(GridItem(label));
					break;
				}
				case 4:
				case 5:
					grid.items.add(GridItem(pressKnobs[storeButtons++]));
					break;
				case 6:
					grid.items.add(GridItem(pressKnobs[presetButtons++]));
					break;
				case 7:
				case 8:
					grid.items.add(GridItem(pressKnobs[lowerButtons++]));
					break;

				}
			}
		}
	}
	grid.performLayout(area);
}

void BCR2000_Component::setRotaryParam(int knobNumber, TypedNamedValue *param)
{
	jassert(knobNumber > 0 && knobNumber <= rotaryKnobs.size());
	jassert(param != nullptr);

	rotaryKnobs[knobNumber - 1]->setSynthParameter(param);
}

void BCR2000_Component::setButtonParam(int knobNumber, std::string const &name)
{
	jassert(knobNumber > 0 && knobNumber <= 48 + 14);
	if (knobNumber < 32) {
		// Button as part of Encoder
		auto withButton = dynamic_cast<RotaryWithLabelAndButtonFunction *>(rotaryKnobs[knobNumber - 1]);
		jassert(withButton);
		if (withButton) {
			withButton->setButtonSynthParameter(name);
		}
	}
	else {
		// Standalone button
		pressKnobs[knobNumber - 1 - 32]->setButtonText(name);
	}
}

BCR2000_Component::UpdateSynthListener::UpdateSynthListener(BCR2000_Component* papa) : papa_(papa)
{
	midikraft::MidiController::instance()->addMessageHandler(midiHandler_, [this](MidiInput* source, MidiMessage const& message) {
		listenForMidiMessages(source, message);
	});
}

BCR2000_Component::UpdateSynthListener::~UpdateSynthListener()
{
	midikraft::MidiController::instance()->removeMessageHandler(midiHandler_);
}

void BCR2000_Component::UpdateSynthListener::valueTreePropertyChanged(ValueTree& treeWhosePropertyHasChanged, const Identifier& property)
{
	auto detailedParameters = dynamic_cast<midikraft::DetailedParametersCapability*>(UIModel::currentSynthOfPatch());
	if (detailedParameters) {
		std::string paramName = property.toString().toStdString();
		for (auto param : detailedParameters->allParameterDefinitions()) {
			if (param->name() == paramName) {
				// First thing - update our internal patch model with the new value. This only works for int capabilities
				auto intValueCap = std::dynamic_pointer_cast<midikraft::SynthIntParameterCapability>(param);
				if (intValueCap) {
					if (patch_) {
						intValueCap->setInPatch(*patch_, treeWhosePropertyHasChanged.getProperty(property));
					}
					else {
						// I need some kind of init patch per synth, else it is unclear what the editor should send
					}
				}
				else {
					jassertfalse;
				}

				auto liveUpdater = std::dynamic_pointer_cast<midikraft::SynthParameterLiveEditCapability>(param);
				if (liveUpdater) {
					if (patch_) {
						MidiBuffer messages = liveUpdater->setValueMessages(*patch_, UIModel::currentSynthOfPatch());
						SimpleLogger::instance()->postMessage("Sending messages to synth");

						auto location = dynamic_cast<midikraft::MidiLocationCapability*>(UIModel::currentSynthOfPatch());
						if (location) {
							midikraft::MidiController::instance()->getMidiOutput(location->midiOutput())->sendBlockOfMessagesNow(messages);
						}
						else {
							SimpleLogger::instance()->postMessage("Error: Synth does not provide location information, can't send data to it");
						}
					}
					else {
						SimpleLogger::instance()->postMessage("Error: No patch loaded, can't calculate update messages");
					}
					return;
				}
				else {
					SimpleLogger::instance()->postMessage("Error: Parameter does not implement SynthParameterLiveEditCapability, can't update synth");
					return;
				}
			}
		}
		SimpleLogger::instance()->postMessage("Error, failed to find parameter definition for property " + property.toString());
	}
	else {
		SimpleLogger::instance()->postMessage("Can't update, Synth does not support DetailedParamtersCapability");
	}
}

void BCR2000_Component::UpdateSynthListener::listenForMidiMessages(MidiInput* source, MidiMessage message)
{
	auto synth = UIModel::currentSynthOfPatch();
	auto location = dynamic_cast<midikraft::MidiLocationCapability *>(synth);
	if (!location || location->midiInput() == source->getName().toStdString()) {
		auto syncCap = dynamic_cast<midikraft::BidirectionalSyncCapability*>(synth);
		if (syncCap) {
			int outValue;
			midikraft::SynthParameterDefinition* param;
			if (syncCap->determineParameterChangeFromSysex({ message }, &param, outValue)) {
				papa_->uiValueTree_.setPropertyExcludingListener(this, Identifier(param->name()), outValue, nullptr);
			}
		}
		if (message.isProgramChange() && (!location || location->channel().toOneBasedInt() == message.getChannel())) {
			auto programChangeCap = dynamic_cast<midikraft::SendsProgramChangeCapability*>(synth);
			if (programChangeCap) {
				programChangeCap->gotProgramChange(MidiProgramNumber::fromZeroBase(message.getProgramChangeNumber()));
				if (location) {
					papa_->librarian_.downloadEditBuffer(midikraft::MidiController::instance()->getMidiOutput(location->midiOutput()), 
						UIModel::currentSynthOfPatchSmart(), nullptr, [this](std::vector<midikraft::PatchHolder> patch) {
						if (patch.size() > 0 && patch[0].patch()) {
							updateAllKnobsFromPatch(patch[0].patch());
						}
					});

				}
			}
		}
	}
}

void BCR2000_Component::UpdateSynthListener::updateAllKnobsFromPatch(std::shared_ptr<midikraft::DataFile> newPatch)
{
	patch_ = newPatch;

	auto detailedParameters = std::dynamic_pointer_cast<midikraft::DetailedParametersCapability>(newPatch);
	if (detailedParameters) {
		for (auto param : detailedParameters->allParameterDefinitions()) {
			auto intParam = std::dynamic_pointer_cast<midikraft::SynthIntParameterCapability>(param);
			if (intParam) {
				int value;
				if (intParam->valueInPatch(*newPatch, value)) {
					if (papa_->uiValueTree_.hasProperty(Identifier(param->name()))) {
						papa_->uiValueTree_.setPropertyExcludingListener(this, Identifier(param->name()), value, nullptr);
					}
				}
			}
		}
	}

}

BCR2000_Component::UpdateControllerListener::UpdateControllerListener(BCR2000_Component* papa) : papa_(papa)
{
	midikraft::MidiController::instance()->addMessageHandler(midiHandler_, [this](MidiInput* source, MidiMessage const& message) {
		listenForMidiMessages(source, message);
	});
}

BCR2000_Component::UpdateControllerListener::~UpdateControllerListener()
{
	midikraft::MidiController::instance()->removeMessageHandler(midiHandler_);
}

void BCR2000_Component::UpdateControllerListener::listenForMidiMessages(MidiInput *source, MidiMessage message) {
	// Check if that is a message we need to take seriously
	if (source->getName().toStdString() == papa_->bcr2000_->midiInput()) {
		// This at least is a message from our controller
		auto detailedParameters = dynamic_cast<midikraft::DetailedParametersCapability*>(UIModel::currentSynthOfPatch());
		for (auto param : detailedParameters->allParameterDefinitions()) {
			auto controllerSync = std::dynamic_pointer_cast<midikraft::SynthParameterControllerMapping>(param);
			if (controllerSync) {
				int newValue;
				if (controllerSync->messagesMatchParameter({ message }, newValue)) {
					papa_->uiValueTree_.setPropertyExcludingListener(this, Identifier(param->name()), newValue, nullptr);
				}
			}
		}
	}
}

void BCR2000_Component::UpdateControllerListener::valueTreePropertyChanged(ValueTree& treeWhosePropertyHasChanged, const Identifier& property)
{
	// This will be hit when the UI is changed, either by a new patch or sysex data from the synth, or from moving the UI slider itself
	// In any case, we will create controller update messages and send them to the controller
	auto detailedParameters = dynamic_cast<midikraft::DetailedParametersCapability*>(UIModel::currentSynthOfPatch());
	if (detailedParameters) {
		int newValue = treeWhosePropertyHasChanged.getProperty(property);
		std::string paramName = property.toString().toStdString();
		for (auto param : detailedParameters->allParameterDefinitions()) {
			if (param->name() == paramName) {
				// Now we need to find the CC mapping, and send it to the controller!
				auto controllerSync = std::dynamic_pointer_cast<midikraft::SynthParameterControllerMapping>(param);
				if (controllerSync) {
					if (papa_->bcr2000_->channel().isValid()) {
						auto updateMessage = controllerSync->createParameterMessages(newValue, papa_->bcr2000_->channel());
						midikraft::MidiController::instance()->getMidiOutput(papa_->bcr2000_->midiOutput())->sendBlockOfMessagesNow(MidiHelpers::bufferFromMessages(updateMessage));
					}
					SimpleLogger::instance()->postMessage("Updating controller with message");
					return;
				}
			}
		}
		jassertfalse;
	}
}
