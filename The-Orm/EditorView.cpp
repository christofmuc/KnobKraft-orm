/*
   Copyright (c) 2020-2025 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "EditorView.h"

#include "RotaryWithLabel.h"

#include "MidiController.h"
#include "LambdaValueListener.h"
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
#include <optional>
#include "SpdLogJuce.h"

// See https://www.sequencer.de/synth/index.php/B-Control-Tokenreferenz for the button layout info
static std::map<int, std::string> defaultLabels = {
	{ 57, "Group 1"}, { 58, "Group 2"}, { 59, "Group 3"}, { 60, "Group 4"},
	{ 53, "STORE"}, { 54, "LEARN"}, { 55, "EDIT"}, { 56, "EXIT"},
	{ 63, "Preset <"}, { 64, "Preset >"},
};

namespace {
const juce::Identifier kAssignmentsRootId("SynthAssignments");
const juce::Identifier kSynthNodeId("Synth");
const juce::Identifier kLayoutNodeId("Layout");
const juce::Identifier kRotariesNodeId("Rotaries");
const juce::Identifier kPressesNodeId("Presses");
const juce::Identifier kAssignmentNodeId("Assignment");
const juce::Identifier kSynthNameProperty("synthName");
const juce::Identifier kLayoutIdProperty("layoutId");
const juce::Identifier kIndexProperty("index");
const juce::Identifier kParameterProperty("parameter");
}

EditorView::EditorView(std::shared_ptr<midikraft::BCR2000> bcr)
	: patchTextBox_([this]() { resized(); }, true)
	, updateSynthListener_(this)
	, bcr2000_(bcr)
	, librarian_({})
{
	// Create the value tree viewer that shows the internals of the parameters defined
	addAndMakeVisible(valueTreeViewer_);
	addAndMakeVisible(&patchTextBox_);
	patchTextBox_.fillTextBox(nullptr);
	assignmentsRoot_ = ValueTree(kAssignmentsRootId);
	valueTreeViewer_.setPropertyColourFunction([this](const juce::ValueTree&, juce::Identifier propertyId, bool) -> std::optional<juce::Colour> {
		auto propertyName = propertyId.toString().toStdString();
		if (!uiModel_.hasValue(propertyName)) {
			return std::nullopt;
		}
		if (assignmentUsage_.find(propertyName) == assignmentUsage_.end()) {
			return juce::Colours::orange;
		}
		return std::nullopt;
	});

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
	rotaryAssignmentNames_.resize(rotaryKnobs.size());
	// Create 64 "press knobs", that includes those buttons that are on the encoders and those on the right side of the BCR2000
	for (int i = 0; i < 64; i++) {
		auto press = new ToggleButton();
		press->setClickingTogglesState(true);
		press->onClick = [this, buttonIndex = i]() { handlePressButtonClick(buttonIndex); };
		if (defaultLabels.find(i + 1) != defaultLabels.end()) press->setButtonText(defaultLabels[i + 1]);
		pressKnobs.add(press);
		addAndMakeVisible(press);
		defaultPressTexts_.push_back(press->getButtonText());
	}
	pressBindings_.resize(pressKnobs.size());
	pressAssignmentNames_.resize(pressKnobs.size());
	clearPressBindings();

	// Extra function buttons
	LambdaButtonStrip::TButtonMap buttons = {
		{ "loadAssignments", { "Load layout", [this]() { handleLoadAssignmentsRequested(); } } },
		{ "saveAssignments", { "Save layout", [this]() { handleSaveAssignmentsRequested(); } } }
	};
	buttons_ = std::make_unique<LambdaButtonStrip>(505, LambdaButtonStrip::Direction::Horizontal);
	buttons_->setButtonDefinitions(buttons);
	addAndMakeVisible(*buttons_);

	loadAssignmentsFromDisk();

	// Finally make sure we get notified if the current synth changes
	UIModel::instance()->currentSynth_.addChangeListener(this);
	UIModel::instance()->currentPatch_.addChangeListener(this);
	UIModel::instance()->currentPatchValues_.addChangeListener(this);
}

EditorView::~EditorView()
{
	flushAssignmentsIfDirty();
	UIModel::instance()->currentPatchValues_.removeChangeListener(this);
	UIModel::instance()->currentPatch_.removeChangeListener(this);
	UIModel::instance()->currentSynth_.removeChangeListener(this);
}

void EditorView::changeListenerCallback(ChangeBroadcaster* source) {
	CurrentSynth* current = dynamic_cast<CurrentSynth*>(source);
	if (current) {
		flushAssignmentsIfDirty();
		currentSynthName_.clear();
		currentLayoutNode_ = {};

		// Current synth was changed, reset UI and rebuild editor
		for (int i = 0; i < rotaryKnobs.size(); ++i) {
			if (auto* knob = rotaryKnobs[i]) {
				knob->setUnused();
			}
			if (i < static_cast<int>(rotaryAssignmentNames_.size())) {
				replaceAssignmentName(rotaryAssignmentNames_[i], "");
			}
		}
		clearPressBindings();

		auto synthPtr = current->smartSynth();
		if (synthPtr) {
			currentSynthName_ = synthPtr->getName();
		}

		auto supported = midikraft::Capability::hasCapability<midikraft::SynthParametersCapability>(current->smartSynth());
		if (supported) {
			// Create the TypedNamedValuesSet that describes the paramters of the currently selected synth
			uiModel_ = createParameterModel();

			// Add all Values of the uiModel_ into a ValueTree
			uiValueTree_.removeListener(&updateSynthListener_);
			uiValueTree_ = ValueTree("UIMODEL");
			uiModel_.addToValueTree(uiValueTree_);
		
			// Setup the left side tree view to see the raw ValueTree
			valueTreeViewer_.setValueTree(uiValueTree_);

			//Old: supported->setupBCR2000View(this, uiModel_, uiValueTree_);

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

			// Now attach a sysex generating listener to the values of the ValueTree
			uiValueTree_.addListener(&updateSynthListener_);

			loadAssignmentsForSynth(current->smartSynth());
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

	// Left side for tree control, right side for patch text display
	auto sidebarWidth = area.getWidth() / 4;
	valueTreeViewer_.setBounds(area.removeFromLeft(sidebarWidth));
	patchTextBox_.setBounds(area.removeFromRight(sidebarWidth));

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

void EditorView::paintOverChildren(juce::Graphics& g)
{
	auto drawHighlight = [&g](juce::Component* component) {
		if (component == nullptr) {
			return;
		}
		auto bounds = component->getBounds().toFloat().reduced(2.0f);
		g.setColour(juce::Colours::orange.withAlpha(0.2f));
		g.fillRoundedRectangle(bounds, 6.0f);
		g.setColour(juce::Colours::orange.withAlpha(0.8f));
		g.drawRoundedRectangle(bounds, 6.0f, 2.0f);
	};

	if (hoveredRotaryIndex_ >= 0 && hoveredRotaryIndex_ < rotaryKnobs.size()) {
		drawHighlight(rotaryKnobs[hoveredRotaryIndex_]);
	}
	if (hoveredPressIndex_ >= 0 && hoveredPressIndex_ < pressKnobs.size()) {
		drawHighlight(pressKnobs[hoveredPressIndex_]);
	}
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
		auto index = knobNumber - 1 - 32;
		pressKnobs[index]->setButtonText(name);
		if (index >= 0 && index < static_cast<int>(defaultPressTexts_.size())) {
			defaultPressTexts_[index] = pressKnobs[index]->getButtonText();
		}
	}
}

bool EditorView::isInterestedInDragSource(const juce::DragAndDropTarget::SourceDetails& details)
{
	auto param = findParameterByName(details.description.toString());
	return param != nullptr;
}

void EditorView::itemDragEnter(const juce::DragAndDropTarget::SourceDetails& details)
{
	updateDropHoverState(details);
}

void EditorView::itemDragMove(const juce::DragAndDropTarget::SourceDetails& details)
{
	updateDropHoverState(details);
}

void EditorView::itemDragExit(const juce::DragAndDropTarget::SourceDetails& details)
{
	juce::ignoreUnused(details);
	clearDropHoverState();
}

void EditorView::itemDropped(const juce::DragAndDropTarget::SourceDetails& details)
{
	auto param = findParameterByName(details.description.toString());
	if (!param) {
		return;
	}

	auto localPos = mousePositionInLocalSpace();

	if (auto rotaryIndex = rotaryIndexAt(localPos); rotaryIndex != -1) {
		assignParameterToRotary(rotaryIndex, param);
		return;
	}

	if (auto pressIndex = pressIndexAt(localPos); pressIndex != -1) {
		assignParameterToPress(pressIndex, param);
	}
	clearDropHoverState();
}

void EditorView::dragOperationEnded(const juce::DragAndDropTarget::SourceDetails& details)
{
	DragAndDropContainer::dragOperationEnded(details);
	clearDropHoverState();
}

void EditorView::setEditorPatch(std::shared_ptr<midikraft::Synth> synth, std::shared_ptr<midikraft::DataFile> data)
{
	if (synth && data) {
		editorPatchHolder_ = std::make_shared<midikraft::PatchHolder>(synth, nullptr, data);
	}
	else {
		editorPatchHolder_.reset();
	}
	patchTextBox_.fillTextBox(editorPatchHolder_);
}

void EditorView::refreshEditorPatch()
{
	if (editorPatchHolder_) {
		patchTextBox_.fillTextBox(editorPatchHolder_);
	}
}

EditorView::UpdateSynthListener::UpdateSynthListener(EditorView* papa) : papa_(papa)
{
	// We need a real edit buffer now
	editBuffer_ = std::make_shared<midikraft::DataFile>(0);

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
	auto detailedParameters = midikraft::Capability::hasCapability<midikraft::SynthParametersCapability>(UIModel::currentSynthOfPatchSmart());
	if (detailedParameters) {
		std::string paramName = property.toString().toStdString();
		bool found = false;
		for (auto param : detailedParameters->getParameterDefinitions()) {
			if (param.name == paramName) {
				// First thing - update our internal patch model with the new value. 
				midikraft::ParamVal newValue{ param.param_id, treeWhosePropertyHasChanged.getProperty(property) };
				detailedParameters->setParameterValues(editBuffer_, { newValue });

				// Second thing - create MIDI messages that will update the real synth with the new value
				auto location = std::dynamic_pointer_cast<midikraft::SimpleDiscoverableDevice>(UIModel::currentSynthOfPatchSmart());
				MidiChannel channel = location ? location->channel() : MidiChannel::invalidChannel();
				auto messages = detailedParameters->createSetValueMessages(channel, editBuffer_, { param.param_id });
				if (messages.size() > 0) {
					if (location->wasDetected()) {
						// Send messages, should be visible in MIDI log
						UIModel::currentSynthOfPatch()->sendBlockOfMessagesToSynth(location->midiOutput(), messages);
					}
					else {
						String valueAsText = newValue.value.toString();
						spdlog::info("Synth {} not detected, can't send message to update {} to new value {}: {}",
							UIModel::currentSynthOfPatch()->getName(), param.name, valueAsText.toStdString(), Sysex::dumpSysexToString(messages));
					}
				}
				else {
					spdlog::info("Adaptation did not generate MIDI messages to update synth, check implementation of createSetValueMessages()");
				}
				papa_->refreshEditorPatch();
				found = true;
				break;
			}
		}
		if (!found) {
			spdlog::error("Failed to find parameter definition for property {}", property.toString());
		}
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

std::optional<midikraft::ParamVal> valueForParameter(midikraft::ParamDef const& param, std::vector<midikraft::ParamVal> const& values) {
	for (auto const& val : values) {
		if (val.param_id == param.param_id) {
			return val;
		}
	}
	return {};
}

void EditorView::UpdateSynthListener::updateAllKnobsFromPatch(std::shared_ptr<midikraft::Synth> synth, std::shared_ptr<midikraft::DataFile> newPatch)
{
	auto detailedParameters = midikraft::Capability::hasCapability<midikraft::SynthParametersCapability>(synth);
	if (detailedParameters) {
		if (newPatch) {
			// Copy the new patch into the edit buffer of the editor
			editBuffer_->setData(newPatch->data());
			auto values = detailedParameters->getParameterValues(editBuffer_, false);
			for (auto param : detailedParameters->getParameterDefinitions()) {
				auto value_determined = valueForParameter(param, values);
				if (value_determined.has_value()) {
					switch (param.param_type) {
					case midikraft::ParamType::VALUE:
						// Nothing to do, we can use the value verbatim
						break;
					case midikraft::ParamType::CHOICE:
					{
						// Choice value retrieved - this might be a string, and we have to find the index of the string in the list of possible values
						auto valueArray = param.values.getArray();
						juce::var clearTextValue = value_determined.value().value;
						value_determined.reset();
						if (valueArray) {
							int index = 0;
							for (auto elementPtr = valueArray->begin(); elementPtr != valueArray->end(); elementPtr++) {
								if (*elementPtr == clearTextValue) {
									value_determined = midikraft::ParamVal({ param.param_id, juce::var(index) });
									break;
								}
								index++;
							}
						}
						break;
					}
					default:
						spdlog::warn("parameter type not yet implemented for parameter {}", param.name);
					}
				}

				if (value_determined.has_value()) {
					if (papa_->uiValueTree_.hasProperty(Identifier(param.name))) {
						papa_->uiValueTree_.setPropertyExcludingListener(this, Identifier(param.name), (int)value_determined.value().value, nullptr);
					}
				}
			}
		}
		else {
			// This synth has not defined a patch, no init patch is available. Reset the editor values to their minimum value
			for (auto param : detailedParameters->getParameterDefinitions()) {
				if (papa_->uiValueTree_.hasProperty(Identifier(param.name))) {
					switch (param.param_type) {
					case midikraft::ParamType::VALUE:
						papa_->uiValueTree_.setPropertyExcludingListener(this, Identifier(param.name), (int)(param.values[0]), nullptr);
						break;
					case midikraft::ParamType::CHOICE:
						// Simply set it to index 0, the first entry
						papa_->uiValueTree_.setPropertyExcludingListener(this, Identifier(param.name), 0, nullptr);
						break;
					default:
						// Not supported
						break;
					}
				}
			}
			papa_->setEditorPatch(nullptr, nullptr);
			return;
		}
		papa_->setEditorPatch(synth, editBuffer_);
	}
}

std::shared_ptr<TypedNamedValue> EditorView::findParameterByName(const juce::String& propertyName)
{
	auto trimmed = propertyName.trim();
	if (trimmed.isEmpty()) {
		return {};
	}
	auto name = trimmed.toStdString();
	if (!uiModel_.hasValue(name)) {
		return {};
	}
	return uiModel_.typedNamedValueByName(name);
}

void EditorView::assignParameterToRotary(int rotaryIndex, std::shared_ptr<TypedNamedValue> param, bool updateStorage)
{
	if (!param || rotaryIndex < 0 || rotaryIndex >= rotaryKnobs.size()) {
		return;
	}

	setRotaryParam(rotaryIndex + 1, param.get());

	if (rotaryAssignmentNames_.size() != static_cast<size_t>(rotaryKnobs.size())) {
		rotaryAssignmentNames_.resize(rotaryKnobs.size());
	}
	replaceAssignmentName(rotaryAssignmentNames_[rotaryIndex], param->name().toStdString());

	if (auto* knob = rotaryKnobs[rotaryIndex]) {
		auto valueVar = param->value().getValue();
		double sliderValue = 0.0;
		if (valueVar.isDouble()) {
			sliderValue = static_cast<double>(valueVar);
		}
		else if (valueVar.isInt() || valueVar.isInt64()) {
			sliderValue = static_cast<double>(static_cast<int>(valueVar));
		}
		else if (valueVar.isBool()) {
			sliderValue = static_cast<bool>(valueVar) ? 1.0 : 0.0;
		}
		else {
			sliderValue = static_cast<double>(param->minValue());
		}
		auto sliderInt = juce::roundToInt(sliderValue);
		knob->setValue(sliderInt);
	}

	if (updateStorage) {
		storeRotaryAssignment(rotaryIndex, param);
		markAssignmentsDirty();
		updateAssignmentHighlight();
	}
}

void EditorView::assignParameterToPress(int pressIndex, std::shared_ptr<TypedNamedValue> param, bool updateStorage)
{
	if (!param || pressIndex < 0 || pressIndex >= pressKnobs.size()) {
		return;
	}

	if (!canAssignToPress(*param)) {
		spdlog::warn("Parameter {} is not two-valued, can't assign to button {}", param->name().toStdString(), pressIndex + 1);
		return;
	}

	auto* button = pressKnobs[pressIndex];
	auto& binding = pressBindings_[pressIndex];
	binding.listener.reset();

	bool usesBool = param->valueType() == ValueType::Bool;
	int offValue = 0;
	int onValue = 1;
	if (!usesBool) {
		if (!extractBinaryValues(*param, offValue, onValue)) {
			return;
		}
	}

	binding.param = param;
	binding.usesBool = usesBool;
	binding.offValue = usesBool ? 0 : offValue;
	binding.onValue = usesBool ? 1 : onValue;

	binding.listener = std::make_unique<LambdaValueListener>(param->value(), [this, pressIndex](juce::Value&) {
		refreshPressButton(pressIndex);
	});

	refreshPressButton(pressIndex);
	button->setTooltip("Controls " + param->name());

	if (pressAssignmentNames_.size() != static_cast<size_t>(pressKnobs.size())) {
		pressAssignmentNames_.resize(pressKnobs.size());
	}
	replaceAssignmentName(pressAssignmentNames_[pressIndex], param->name().toStdString());

	if (updateStorage) {
		storePressAssignment(pressIndex, param);
		markAssignmentsDirty();
		updateAssignmentHighlight();
	}
}

bool EditorView::canAssignToPress(const TypedNamedValue& param) const
{
	if (param.valueType() == ValueType::Bool) {
		return true;
	}
	int offValue = 0;
	int onValue = 0;
	return extractBinaryValues(param, offValue, onValue);
}

bool EditorView::extractBinaryValues(const TypedNamedValue& param, int& offValue, int& onValue) const
{
	switch (param.valueType()) {
	case ValueType::Integer:
		if (param.maxValue() - param.minValue() == 1) {
			offValue = param.minValue();
			onValue = param.maxValue();
			return true;
		}
		break;
	case ValueType::Lookup: {
		auto lookup = param.lookup();
		if (lookup.size() == 2) {
			auto it = lookup.begin();
			offValue = it->first;
			++it;
			onValue = it->first;
			return true;
		}
		break;
	}
	default:
		break;
	}
	return false;
}

void EditorView::refreshPressButton(int pressIndex)
{
	if (pressIndex < 0 || pressIndex >= pressKnobs.size()) {
		return;
	}
	auto& binding = pressBindings_[pressIndex];
	if (!binding.param) {
		return;
	}

	auto* button = pressKnobs[pressIndex];
	auto valueVar = binding.param->value().getValue();

	bool isOn = false;
	if (binding.param->valueType() == ValueType::Bool) {
		isOn = static_cast<bool>(valueVar);
	}
	else {
		int currentValue = static_cast<int>(valueVar);
		isOn = (currentValue == binding.onValue);
	}

	button->setToggleState(isOn, juce::dontSendNotification);
	button->setButtonText(binding.param->name() + ": " + buttonValueText(*binding.param, valueVar));
}

void EditorView::handlePressButtonClick(int pressIndex)
{
	if (pressIndex < 0 || pressIndex >= pressKnobs.size()) {
		return;
	}
	auto& binding = pressBindings_[pressIndex];
	auto* button = pressKnobs[pressIndex];
	if (!binding.param) {
		button->setToggleState(false, juce::dontSendNotification);
		return;
	}

	bool shouldBeOn = button->getToggleState();
	if (binding.param->valueType() == ValueType::Bool) {
		binding.param->value().setValue(shouldBeOn);
	}
	else {
		binding.param->value().setValue(shouldBeOn ? binding.onValue : binding.offValue);
	}
}

juce::String EditorView::buttonValueText(const TypedNamedValue& param, const juce::var& value) const
{
	switch (param.valueType()) {
	case ValueType::Bool:
		return (bool)value ? "On" : "Off";
	case ValueType::Lookup: {
		auto lookup = param.lookup();
		int intValue = static_cast<int>(value);
		auto found = lookup.find(intValue);
		if (found != lookup.end()) {
			return found->second;
		}
		break;
	}
	default:
		break;
	}
	if (value.isString()) {
		return value.toString();
	}
	if (value.isDouble() || value.isInt()) {
		return juce::String((int)value);
	}
	return value.toString();
}

int EditorView::rotaryIndexAt(juce::Point<int> localPos) const
{
	for (int i = 0; i < rotaryKnobs.size(); ++i) {
		if (auto* knob = rotaryKnobs[i]) {
			if (knob->isShowing() && knob->getBounds().contains(localPos)) {
				return i;
			}
		}
	}
	return -1;
}

int EditorView::pressIndexAt(juce::Point<int> localPos) const
{
	for (int i = 0; i < pressKnobs.size(); ++i) {
		if (auto* button = pressKnobs[i]) {
			if (button->isShowing() && button->getBounds().contains(localPos)) {
				return i;
			}
		}
	}
	return -1;
}

juce::Point<int> EditorView::mousePositionInLocalSpace() const
{
	auto screenPos = juce::Desktop::getInstance().getMainMouseSource().getScreenPosition();
	return getLocalPoint(nullptr, screenPos.roundToInt());
}

void EditorView::updateDropHoverState(const juce::DragAndDropTarget::SourceDetails& details)
{
	auto localPos = mousePositionInLocalSpace();
	int newRotary = -1;
	int newPress = -1;
	bool canDrop = false;

	auto param = findParameterByName(details.description.toString());
	if (param) {
		if (auto idx = rotaryIndexAt(localPos); idx != -1) {
			newRotary = idx;
			canDrop = true;
		}
		else if (auto idx2 = pressIndexAt(localPos); idx2 != -1) {
			if (canAssignToPress(*param)) {
				newPress = idx2;
				canDrop = true;
			}
		}
	}

	if (newRotary != hoveredRotaryIndex_ || newPress != hoveredPressIndex_) {
		hoveredRotaryIndex_ = newRotary;
		hoveredPressIndex_ = newPress;
		repaint();
	}

	setMouseCursor(canDrop ? juce::MouseCursor::CopyingCursor : juce::MouseCursor::NormalCursor);
}

void EditorView::clearDropHoverState()
{
	bool hadHover = hoveredRotaryIndex_ != -1 || hoveredPressIndex_ != -1;
	hoveredRotaryIndex_ = -1;
	hoveredPressIndex_ = -1;
	setMouseCursor(juce::MouseCursor::NormalCursor);
	if (hadHover) {
		repaint();
	}
}

void EditorView::loadAssignmentsForSynth(std::shared_ptr<midikraft::Synth> synth)
{
	currentLayoutNode_ = {};

	if (!synth) {
		return;
	}

	if (!assignmentsLoaded_) {
		loadAssignmentsFromDisk();
	}

	auto synthName = juce::String(synth->getName());
	currentLayoutNode_ = findLayoutNode(synthName);
	if (!currentLayoutNode_.isValid()) {
		currentLayoutNode_ = ensureLayoutNode(synthName);
		return;
	}

	applyAssignmentsToCurrentSynth();
}

void EditorView::applyAssignmentsToCurrentSynth()
{
	// Ensure UI starts from a clean state before applying stored assignments
	if (rotaryAssignmentNames_.size() != static_cast<size_t>(rotaryKnobs.size())) {
		rotaryAssignmentNames_.resize(rotaryKnobs.size());
	}
	for (int i = 0; i < rotaryKnobs.size(); ++i) {
		if (auto* knob = rotaryKnobs[i]) {
			knob->setUnused();
		}
		if (i < static_cast<int>(rotaryAssignmentNames_.size())) {
			replaceAssignmentName(rotaryAssignmentNames_[i], "");
		}
	}
	clearPressBindings();

	if (currentLayoutNode_.isValid()) {
		applyAssignmentsFromTree(currentLayoutNode_);
	}
	else {
		updateAssignmentHighlight();
	}
}

void EditorView::applyAssignmentsFromTree(juce::ValueTree const& layoutTree)
{
	auto rotaries = layoutTree.getChildWithName(kRotariesNodeId);
	if (rotaries.isValid()) {
		for (int i = 0; i < rotaries.getNumChildren(); ++i) {
			applyAssignmentToRotaryFromTree(rotaries.getChild(i));
		}
	}

	auto presses = layoutTree.getChildWithName(kPressesNodeId);
	if (presses.isValid()) {
		for (int i = 0; i < presses.getNumChildren(); ++i) {
			applyAssignmentToPressFromTree(presses.getChild(i));
		}
	}

	updateAssignmentHighlight();
}

void EditorView::applyAssignmentToRotaryFromTree(juce::ValueTree const& assignmentNode)
{
	if (!assignmentNode.hasProperty(kIndexProperty) || !assignmentNode.hasProperty(kParameterProperty)) {
		return;
	}

	int index = static_cast<int>(assignmentNode.getProperty(kIndexProperty));
	if (index < 0 || index >= rotaryKnobs.size()) {
		return;
	}

	auto paramName = assignmentNode.getProperty(kParameterProperty).toString().toStdString();
	if (!uiModel_.hasValue(paramName)) {
		spdlog::warn("Stored rotary assignment references unknown parameter {}", paramName);
		return;
	}

	assignParameterToRotary(index, uiModel_.typedNamedValueByName(paramName), false);
}

void EditorView::applyAssignmentToPressFromTree(juce::ValueTree const& assignmentNode)
{
	if (!assignmentNode.hasProperty(kIndexProperty) || !assignmentNode.hasProperty(kParameterProperty)) {
		return;
	}

	int index = static_cast<int>(assignmentNode.getProperty(kIndexProperty));
	if (index < 0 || index >= pressKnobs.size()) {
		return;
	}

	auto paramName = assignmentNode.getProperty(kParameterProperty).toString().toStdString();
	if (!uiModel_.hasValue(paramName)) {
		spdlog::warn("Stored button assignment references unknown parameter {}", paramName);
		return;
	}

	assignParameterToPress(index, uiModel_.typedNamedValueByName(paramName), false);
}

void EditorView::storeRotaryAssignment(int rotaryIndex, std::shared_ptr<TypedNamedValue> param)
{
	if (!param || currentSynthName_.isEmpty()) {
		return;
	}

	auto layout = ensureLayoutNode(currentSynthName_);
	currentLayoutNode_ = layout;
	auto rotaries = ensureSection(layout, kRotariesNodeId);
	auto assignment = ensureAssignmentNode(rotaries, rotaryIndex);
	assignment.setProperty(kParameterProperty, param->name(), nullptr);
}

void EditorView::storePressAssignment(int pressIndex, std::shared_ptr<TypedNamedValue> param)
{
	if (!param || currentSynthName_.isEmpty()) {
		return;
	}

	auto layout = ensureLayoutNode(currentSynthName_);
	currentLayoutNode_ = layout;
	auto presses = ensureSection(layout, kPressesNodeId);
	auto assignment = ensureAssignmentNode(presses, pressIndex);
	assignment.setProperty(kParameterProperty, param->name(), nullptr);
}

juce::ValueTree EditorView::ensureLayoutNode(const juce::String& synthName)
{
	if (!assignmentsRoot_.isValid()) {
		assignmentsRoot_ = ValueTree(kAssignmentsRootId);
	}

	auto synthNode = assignmentsRoot_.getChildWithProperty(kSynthNameProperty, synthName);
	if (!synthNode.isValid()) {
		synthNode = ValueTree(kSynthNodeId);
		synthNode.setProperty(kSynthNameProperty, synthName, nullptr);
		assignmentsRoot_.addChild(synthNode, -1, nullptr);
	}

	auto layoutNode = synthNode.getChildWithProperty(kLayoutIdProperty, currentLayoutId_);
	if (!layoutNode.isValid()) {
		layoutNode = ValueTree(kLayoutNodeId);
		layoutNode.setProperty(kLayoutIdProperty, currentLayoutId_, nullptr);
		synthNode.addChild(layoutNode, -1, nullptr);
	}

	return layoutNode;
}

juce::ValueTree EditorView::findLayoutNode(const juce::String& synthName) const
{
	if (!assignmentsRoot_.isValid()) {
		return {};
	}

	auto synthNode = assignmentsRoot_.getChildWithProperty(kSynthNameProperty, synthName);
	if (!synthNode.isValid()) {
		return {};
	}

	return synthNode.getChildWithProperty(kLayoutIdProperty, currentLayoutId_);
}

juce::ValueTree EditorView::ensureSection(juce::ValueTree parent, const juce::Identifier& sectionId)
{
	if (!parent.isValid()) {
		return {};
	}

	auto section = parent.getChildWithName(sectionId);
	if (!section.isValid()) {
		section = ValueTree(sectionId);
		parent.addChild(section, -1, nullptr);
	}
	return section;
}

juce::ValueTree EditorView::findAssignmentNode(juce::ValueTree parent, int index) const
{
	if (!parent.isValid()) {
		return {};
	}

	for (int i = 0; i < parent.getNumChildren(); ++i) {
		auto child = parent.getChild(i);
		if (child.hasProperty(kIndexProperty) && static_cast<int>(child.getProperty(kIndexProperty)) == index) {
			return child;
		}
	}
	return {};
}

juce::ValueTree EditorView::ensureAssignmentNode(juce::ValueTree parent, int index)
{
	auto node = findAssignmentNode(parent, index);
	if (!node.isValid()) {
		node = ValueTree(kAssignmentNodeId);
		node.setProperty(kIndexProperty, index, nullptr);
		parent.addChild(node, -1, nullptr);
	}
	return node;
}

void EditorView::loadAssignmentsFromDisk()
{
	auto file = assignmentsFile();
	if (file.existsAsFile()) {
		FileInputStream stream(file);
		if (stream.openedOk()) {
			auto loaded = ValueTree::readFromStream(stream);
			if (loaded.isValid() && loaded.getType() == kAssignmentsRootId) {
				assignmentsRoot_ = loaded;
			}
			else {
				assignmentsRoot_ = ValueTree(kAssignmentsRootId);
				spdlog::warn("Assignments file {} could not be read, starting with empty assignments", file.getFullPathName().toStdString());
			}
		}
		else {
			spdlog::warn("Failed to open assignments file {}", file.getFullPathName().toStdString());
			assignmentsRoot_ = ValueTree(kAssignmentsRootId);
		}
	}
	else {
		assignmentsRoot_ = ValueTree(kAssignmentsRootId);
	}
	assignmentsLoaded_ = true;
	assignmentsDirty_ = false;
}

void EditorView::saveAssignmentsToDisk()
{
	if (!assignmentsRoot_.isValid()) {
		return;
	}

	auto file = assignmentsFile();
	auto directory = file.getParentDirectory();
	if (!directory.exists()) {
		directory.createDirectory();
	}

	TemporaryFile temp(file);
	{
		FileOutputStream out(temp.getFile());
		if (!out.openedOk()) {
			spdlog::error("Failed to open temporary file for writing assignments {}", temp.getFile().getFullPathName().toStdString());
			return;
		}
		assignmentsRoot_.writeToStream(out);
		out.flush();
		if (out.getStatus().failed()) {
			spdlog::error("Failed while writing assignments file {}: {}", temp.getFile().getFullPathName().toStdString(), out.getStatus().getErrorMessage().toStdString());
			return;
		}
	}

	if (!temp.overwriteTargetFileWithTemporary()) {
		spdlog::error("Failed to move temporary assignments file into place {}", file.getFullPathName().toStdString());
		return;
	}

	assignmentsDirty_ = false;
	spdlog::info("Controller assignments saved to {}", file.getFullPathName().toStdString());
}

void EditorView::handleLoadAssignmentsRequested()
{
	if (assignmentsDirty_) {
		spdlog::info("Discarding unsaved controller assignments before loading from disk");
	}

	loadAssignmentsFromDisk();

	if (currentSynthName_.isEmpty()) {
		return;
	}

	currentLayoutNode_ = findLayoutNode(currentSynthName_);
	applyAssignmentsToCurrentSynth();
}

void EditorView::handleSaveAssignmentsRequested()
{
	if (!assignmentsLoaded_) {
		loadAssignmentsFromDisk();
	}

	if (assignmentsDirty_) {
		saveAssignmentsToDisk();
	}
	else {
		spdlog::info("Controller assignments unchanged, nothing to save");
	}
}

juce::File EditorView::assignmentsFile() const
{
	auto& settingsFile = Settings::instance().getPropertiesFile();
	auto directory = settingsFile.getParentDirectory();
	return directory.getChildFile("KnobAssignments.xml");
}

void EditorView::markAssignmentsDirty()
{
	assignmentsDirty_ = true;
}

void EditorView::flushAssignmentsIfDirty()
{
	if (assignmentsDirty_) {
		saveAssignmentsToDisk();
	}
}

void EditorView::clearPressBindings()
{
	if (pressAssignmentNames_.size() != pressBindings_.size()) {
		pressAssignmentNames_.resize(pressBindings_.size());
	}
	for (int i = 0; i < static_cast<int>(pressBindings_.size()); ++i) {
		auto& binding = pressBindings_[i];
		if (binding.listener) {
			binding.listener.reset();
		}
		binding.param.reset();
		binding.usesBool = false;
		binding.offValue = 0;
		binding.onValue = 1;
		if (i < static_cast<int>(pressAssignmentNames_.size())) {
			replaceAssignmentName(pressAssignmentNames_[i], "");
		}

		if (i < pressKnobs.size()) {
			auto* button = pressKnobs[i];
			button->setToggleState(false, juce::dontSendNotification);
			button->setTooltip({});
			if (i < static_cast<int>(defaultPressTexts_.size())) {
				button->setButtonText(defaultPressTexts_[i]);
			}
			else {
				button->setButtonText({});
			}
		}
	}
	updateAssignmentHighlight();
}

void EditorView::updateAssignmentHighlight()
{
	if (valueTreeViewer_.getValueTree().isValid()) {
		valueTreeViewer_.refresh();
	}
}

void EditorView::incrementAssignment(const std::string& name)
{
	if (name.empty()) {
		return;
	}
	assignmentUsage_[name] += 1;
}

void EditorView::decrementAssignment(const std::string& name)
{
	if (name.empty()) {
		return;
	}
	auto it = assignmentUsage_.find(name);
	if (it != assignmentUsage_.end()) {
		if (--it->second <= 0) {
			assignmentUsage_.erase(it);
		}
	}
}

void EditorView::replaceAssignmentName(std::string& slot, const std::string& newName)
{
	if (slot == newName) {
		return;
	}
	if (!slot.empty()) {
		decrementAssignment(slot);
	}
	slot = newName;
	if (!slot.empty()) {
		incrementAssignment(slot);
	}
}
