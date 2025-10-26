/*
   Copyright (c) 2020-2025 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "EditorView.h"

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
#include "CreateInitPatchDataCapability.h"

#include "MidiHelpers.h"

#include <spdlog/spdlog.h>
#include "SpdLogJuce.h"

// See https://www.sequencer.de/synth/index.php/B-Control-Tokenreferenz for the button layout info
static std::map<int, std::string> defaultLabels = {
	{ 57, "Group 1"}, { 58, "Group 2"}, { 59, "Group 3"}, { 60, "Group 4"},
	{ 53, "STORE"}, { 54, "LEARN"}, { 55, "EDIT"}, { 56, "EXIT"},
	{ 63, "Preset <"}, { 64, "Preset >"},
};

EditorView::EditorView(std::shared_ptr<midikraft::BCR2000> bcr) : updateSynthListener_(this), bcr2000_(bcr), librarian_({})
{
	// Create the value tree viewer that shows the internals of the parameters defined
	addAndMakeVisible(valueTreeViewer_);

	// Create 7*8 rotary knobs for the BCR2000 display
	for (int i = 0; i < 7 * 8; i++) {
		RotaryWithLabel* knob;
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
	/*LambdaButtonStrip::TButtonMap buttons = {
	};
	buttons_ = std::make_unique<LambdaButtonStrip>(505, LambdaButtonStrip::Direction::Horizontal);
	buttons_->setButtonDefinitions(buttons);
	addAndMakeVisible(*buttons_);*/

	// Finally make sure we get notified if the current synth changes
	UIModel::instance()->currentSynth_.addChangeListener(this);
	UIModel::instance()->currentPatch_.addChangeListener(this);
	UIModel::instance()->currentPatchValues_.addChangeListener(this);
}

EditorView::~EditorView()
{
	UIModel::instance()->currentPatchValues_.removeChangeListener(this);
	UIModel::instance()->currentPatch_.removeChangeListener(this);
	UIModel::instance()->currentSynth_.removeChangeListener(this);
}

void EditorView::changeListenerCallback(ChangeBroadcaster* source) {
	CurrentSynth* current = dynamic_cast<CurrentSynth*>(source);
	if (current) {
		// Current synth was changed, reset UI and rebuild editor
		for (auto knob : rotaryKnobs) {
			knob->setUnused();
		}
		auto supported = midikraft::Capability::hasCapability<midikraft::SynthParametersCapability>(current->smartSynth());
		if (supported) {
			// Create the TypedNamedValuesSet that describes the paramters of the currently selected synth
			uiModel_ = createParameterModel();

			// Add all Values of the uiModel_ into a ValueTree
			uiValueTree_ = ValueTree("UIMODEL");
			uiModel_.addToValueTree(uiValueTree_);
		
			// Setup the left side tree view to see the raw ValueTree
			valueTreeViewer_.setValueTree(uiValueTree_);

			//Old: supported->setupBCR2000View(this, uiModel_, uiValueTree_);

			// Now attach a sysex generating listener to the values of the ValueTree
			uiValueTree_.addListener(&updateSynthListener_);

			// This is a new synth - if a patch is loaded, we need to reset it
			auto initPatch = std::dynamic_pointer_cast<midikraft::CreateInitPatchDataCapability>(current->smartSynth());
			if (initPatch) {
				// This synth comes equipped with an init patch, how useful. let's use that.
				auto newPatch = current->smartSynth()->patchFromPatchData(initPatch->createInitPatch(), MidiProgramNumber::fromZeroBase(0));
				updateSynthListener_.updateAllKnobsFromPatch(current->smartSynth(), newPatch);
			}
			else {
				// No init patch defined for this synth, reset the previous patch in the listener should there be one
				updateSynthListener_.updateAllKnobsFromPatch(current->smartSynth(), nullptr);
			}
		}
	}
	else if (dynamic_cast<CurrentPatch*>(source) || source == &UIModel::instance()->currentPatchValues_) {
		updateSynthListener_.updateAllKnobsFromPatch(UIModel::currentPatch().smartSynth(), UIModel::currentPatch().patch());
	}
}


std::shared_ptr<TypedNamedValue> makeTypedNamedValue(midikraft::ParamDef const& param)
{
	switch (param.param_type) {
	case midikraft::ParamType::VALUE:
		if (param.values[0].isInt()) {
			return std::make_shared<TypedNamedValue>(param.name, "Editor", (int) param.values[0], (int)param.values[0], (int)param.values[1]);
		}
		else if (param.values[0].isBool()) {
			return std::make_shared<TypedNamedValue>(param.name, "Editor", (bool) param.values[0]);
		} 
	case midikraft::ParamType::CHOICE:
	{
		std::map<int, std::string> lookup;
		if (param.values.isArray()) {
			auto allowed_values = param.values.getArray();
			for (int i = 0; i < allowed_values->size(); i++) {
				String value = allowed_values->getReference(i);
				lookup.emplace(i, value.toStdString());
			}
		}
		return std::make_shared<TypedNamedValue>(param.name, "Editor", 0, lookup);
	}
	default:
		spdlog::error("Encountered unknown parameter type in automatic editor creation");
	}
	return {};
}


TypedNamedValueSet EditorView::createParameterModel() {
	TypedNamedValueSet result;
	auto detailedParameters = midikraft::Capability::hasCapability<midikraft::SynthParametersCapability>(UIModel::currentSynthOfPatchSmart());
	if (detailedParameters) {
		for (auto param : detailedParameters->getParameterDefinitions()) {
			auto tnv = makeTypedNamedValue(param);
			if (tnv) {
				tnv->value().setValue(tnv->maxValue() + 1); // Deliberately set an invalid value here to force the subsequent update to really refresh all listeners
				// else, the caching of the ValueTree will not update null-valued properties leaving the UI in an inconsistent state.
				result.push_back(tnv);
			}
		}
	}
	return result;
}

void EditorView::resized()
{
	Rectangle<int> area(getLocalBounds());

	// Button strip
	if (buttons_) {
		buttons_->setBounds(area.removeFromBottom(60).reduced(10));
	}

	// Left side for tree control
	valueTreeViewer_.setBounds(area.removeFromLeft(area.getWidth()/4));

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

void EditorView::setRotaryParam(int knobNumber, TypedNamedValue* param)
{
	jassert(knobNumber > 0 && knobNumber <= rotaryKnobs.size());
	jassert(param != nullptr);

	if (param) {
		rotaryKnobs[knobNumber - 1]->setSynthParameter(param);
	}
}

void EditorView::setButtonParam(int knobNumber, std::string const& name)
{
	jassert(knobNumber > 0 && knobNumber <= 48 + 14);
	if (knobNumber < 32) {
		// Button as part of Encoder
		auto withButton = dynamic_cast<RotaryWithLabelAndButtonFunction*>(rotaryKnobs[knobNumber - 1]);
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

EditorView::UpdateSynthListener::UpdateSynthListener(EditorView* papa) : papa_(papa)
{
	midikraft::MidiController::instance()->addMessageHandler(midiHandler_, [this](MidiInput* source, MidiMessage const& message) {
		listenForMidiMessages(source, message);
	});
}

EditorView::UpdateSynthListener::~UpdateSynthListener()
{
	midikraft::MidiController::instance()->removeMessageHandler(midiHandler_);
}

void EditorView::UpdateSynthListener::valueTreePropertyChanged(ValueTree& treeWhosePropertyHasChanged, const Identifier& property)
{
	auto detailedParameters = midikraft::Capability::hasCapability<midikraft::DetailedParametersCapability>(UIModel::currentSynthOfPatchSmart());
	patch_ = UIModel::instance()->currentPatch_.patch().patch();
	if (detailedParameters) {
		std::string paramName = property.toString().toStdString();
		for (auto param : detailedParameters->allParameterDefinitions()) {
			if (param->name() == paramName) {
				// First thing - update our internal patch model with the new value. This only works for int capabilities
				auto intValueCap = midikraft::Capability::hasCapability<midikraft::SynthIntParameterCapability>(param);
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

				auto liveUpdater = midikraft::Capability::hasCapability<midikraft::SynthParameterLiveEditCapability>(param);
				if (liveUpdater) {
					if (patch_) {
						auto messages = liveUpdater->setValueMessages(patch_, UIModel::currentSynthOfPatch());
						auto location = dynamic_cast<midikraft::MidiLocationCapability*>(UIModel::currentSynthOfPatch());
						if (location) {
							spdlog::debug("Sending message to {} to update {} to new value {}",
								UIModel::currentSynthOfPatch()->getName(), param->name(), param->valueInPatchToText(*patch_));
							UIModel::currentSynthOfPatch()->sendBlockOfMessagesToSynth(location->midiOutput(), messages);
						}
						else {
							spdlog::error("Synth does not provide location information, can't send data to it");
						}
					}
					else {
						//SimpleLogger::instance()->postMessage("Error: No patch loaded, can't calculate update messages");
					}
					return;
				}
				else {
					//SimpleLogger::instance()->postMessage("Error: Parameter does not implement SynthParameterLiveEditCapability, can't update synth");
					return;
				}
			}
		}
		spdlog::error("Failed to find parameter definition for property {}", property.toString());
	}
	else {
		//SimpleLogger::instance()->postMessage("Can't update, Synth does not support DetailedParamtersCapability");
	}
}

void EditorView::UpdateSynthListener::listenForMidiMessages(MidiInput* source, MidiMessage message)
{
	auto synth = UIModel::currentSynthOfPatch();
	auto location = dynamic_cast<midikraft::MidiLocationCapability*>(synth);
	if (!location || location->midiInput().name == source->getName()) {
		auto syncCap = dynamic_cast<midikraft::BidirectionalSyncCapability*>(synth);
		if (syncCap) {
			int outValue;
			std::shared_ptr<midikraft::SynthParameterDefinition> param;
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
							updateAllKnobsFromPatch(patch[0].smartSynth(), patch[0].patch());
						}
					});

				}
			}
		}
	}
}

void EditorView::UpdateSynthListener::updateAllKnobsFromPatch(std::shared_ptr<midikraft::Synth> synth, std::shared_ptr<midikraft::DataFile> newPatch)
{
	patch_ = newPatch;
	auto detailedParameters = midikraft::Capability::hasCapability<midikraft::SynthParametersCapability>(synth);
	if (detailedParameters) {
		auto values = detailedParameters->getParameterValues(newPatch, false);
		for (auto param : detailedParameters->getParameterDefinitions()) {
			switch (param.param_type) {
			case midikraft::ParamType::VALUE:
			{
				for (auto const &val : values) {
					if (val.param_id == param.param_id) {
						if (papa_->uiValueTree_.hasProperty(Identifier(param.name))) {
							papa_->uiValueTree_.setPropertyExcludingListener(this, Identifier(param.name), (int) val.value, nullptr);
						}
						break;
					}
				}
			}
			default:
				spdlog::warn("parameter type not yet implemented");
			}
		}
	}
}
