/*
   Copyright (c) 2020-2025 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "EditorView.h"

#include "RotaryWithLabel.h"
#include "LayoutConstants.h"

#include "BidirectionalSyncCapability.h"
#include "CreateInitPatchDataCapability.h"
#include "DetailedParametersCapability.h"
#include "Logger.h"
#include "MidiController.h"
#include "MidiHelpers.h"
#include "MidiProgramNumber.h"
#include "Settings.h"
#include "Synth.h"
#include "SynthParameterDefinition.h"
#include "Sysex.h"
#include "UIModel.h"
#include "SendsProgramChangeCapability.h"

#include <spdlog/spdlog.h>
#include <map>
#include <optional>
#include <cmath>

namespace
{
const juce::Identifier kAssignmentsRootId("SynthAssignments");
const juce::Identifier kSynthNodeId("Synth");
const juce::Identifier kLayoutNodeId("Layout");
const juce::Identifier kSlotsNodeId("Slots");
const juce::Identifier kSlotNodeId("Slot");
const juce::Identifier kSynthNameProperty("synthName");
const juce::Identifier kLayoutIdProperty("layoutId");
const juce::Identifier kIndexProperty("index");
const juce::Identifier kControllerProperty("controller");
const juce::Identifier kParameterProperty("parameter");


class EditorPaletteBackground : public juce::Component
{
public:
    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        juce::DropShadow shadow(kPaletteOutline.withAlpha(0.35f),
                                (int)kPaletteShadowRadius,
                                { 0, (int)kPaletteShadowOffset });
        juce::Path backgroundPath;
        backgroundPath.addRoundedRectangle(bounds.reduced(1.0f), kCornerRadius);
        shadow.drawForPath(g, backgroundPath);

        juce::ColourGradient gradient(kPaletteFillHover,
                                      bounds.getCentreX(),
                                      bounds.getY(),
                                      kPaletteFill,
                                      bounds.getCentreX(),
                                      bounds.getBottom(),
                                      false);
        g.setGradientFill(gradient);
        g.fillPath(backgroundPath);

        g.setColour(kPaletteOutline.withAlpha(0.7f));
        g.strokePath(backgroundPath, juce::PathStrokeType(1.0f));
    }
};


juce::String controllerTypeToString(EditorView::ControllerType type)
{
    return type == EditorView::ControllerType::Rotary ? "rotary" : "button";
}

EditorView::ControllerType stringToControllerType(const juce::String& str)
{
    if (str.compareIgnoreCase("button") == 0)
        return EditorView::ControllerType::Button;
    return EditorView::ControllerType::Rotary;
}

std::shared_ptr<TypedNamedValue> makeTypedNamedValue(midikraft::ParamDef const& param)
{
    switch (param.param_type)
    {
    case midikraft::ParamType::VALUE:
        if (param.values[0].isInt())
            return std::make_shared<TypedNamedValue>(param.name, "Editor", (int)param.values[0], (int)param.values[0], (int)param.values[1]);
        if (param.values[0].isBool())
            return std::make_shared<TypedNamedValue>(param.name, "Editor", (bool)param.values[0]);
        break;
    case midikraft::ParamType::CHOICE:
    {
        std::map<int, std::string> lookup;
        if (param.values.isArray())
        {
            auto allowedValues = param.values.getArray();
            for (int i = 0; i < allowedValues->size(); ++i)
            {
                juce::String value = allowedValues->getReference(i);
                lookup.emplace(i, value.toStdString());
            }
        }
        return std::make_shared<TypedNamedValue>(param.name, "Editor", 0, lookup);
    }
    default:
        spdlog::error("Unknown parameter type in automatic editor creation");
        break;
    }
    return {};
}

} // namespace

//==================================================================================================
// ControllerPaletteItem
//==================================================================================================

EditorView::ControllerPaletteItem::ControllerPaletteItem(EditorView& owner, ControllerType type, juce::String const& labelText)
    : owner_(owner), type_(type), label_(labelText)
{
    setInterceptsMouseClicks(true, true);
}

void EditorView::ControllerPaletteItem::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    auto hover = isMouseOver() || isMouseButtonDown();

    juce::ColourGradient gradient(hover ? kPaletteFillHover : kPaletteFill,
                                  bounds.getCentreX(),
                                  bounds.getY(),
                                  (hover ? kPaletteFill : kPaletteFill.darker(0.05f)),
                                  bounds.getCentreX(),
                                  bounds.getBottom(),
                                  false);
    g.setGradientFill(gradient);
    g.fillRoundedRectangle(bounds, kCornerRadius);

    auto outlineColour = hover ? kAccentColour : kPaletteOutline.withAlpha(0.8f);
    g.setColour(outlineColour);
    g.drawRoundedRectangle(bounds, kCornerRadius, hover ? 2.0f : 1.2f);

    g.setColour(juce::Colours::white.withAlpha(0.92f));
    g.setFont(juce::Font(15.0f, juce::Font::bold));
    g.drawFittedText(label_, bounds.toNearestInt().reduced(LAYOUT_INSET_SMALL), juce::Justification::centred, 1);
}

void EditorView::ControllerPaletteItem::mouseDown(const juce::MouseEvent& event)
{
    juce::ignoreUnused(event);
    if (auto* dragContainer = juce::DragAndDropContainer::findParentDragContainerFor(this))
    {
        const juce::var description = type_ == ControllerType::Rotary ? juce::var("controller:rotary") : juce::var("controller:button");
        dragContainer->startDragging(description, this);
    }
}

void EditorView::ControllerPaletteItem::mouseEnter(const juce::MouseEvent&)
{
    repaint();
}

void EditorView::ControllerPaletteItem::mouseExit(const juce::MouseEvent&)
{
    repaint();
}

//==================================================================================================
// EditorView
//==================================================================================================

EditorView::EditorView(std::shared_ptr<midikraft::BCR2000> bcr)
    : patchTextBox_([this]() { resized(); }, true),
      updateSynthListener_(this),
      bcr2000_(std::move(bcr)),
      librarian_({})
{
    assignmentsRoot_ = juce::ValueTree(kAssignmentsRootId);

    addAndMakeVisible(valueTreeViewer_);
    addAndMakeVisible(patchTextBox_);
    patchTextBox_.fillTextBox(nullptr);

    valueTreeViewer_.setPropertyColourFunction([this](const juce::ValueTree&, juce::Identifier propertyId, bool) -> std::optional<juce::Colour> {
        auto propertyName = propertyId.toString().toStdString();
        if (!uiModel_.hasValue(propertyName))
            return std::nullopt;
        if (assignmentUsage_.find(propertyName) == assignmentUsage_.end())
            return juce::Colours::orange;
        return std::nullopt;
    });

    paletteContainer_ = std::make_unique<EditorPaletteBackground>();
    addAndMakeVisible(*paletteContainer_);
    controllerPaletteItems_.push_back(std::make_unique<ControllerPaletteItem>(*this, ControllerType::Rotary, "Rotary"));
    controllerPaletteItems_.push_back(std::make_unique<ControllerPaletteItem>(*this, ControllerType::Button, "Button"));
    for (auto& item : controllerPaletteItems_)
        paletteContainer_->addAndMakeVisible(item.get());

    totalSlots_ = gridRows_ * gridCols_;
    slots_.resize(totalSlots_);
    rotaryKnobs_.ensureStorageAllocated(totalSlots_);
    buttonControls_.ensureStorageAllocated(totalSlots_);

    for (int slotIndex = 0; slotIndex < totalSlots_; ++slotIndex)
    {
        auto rotary = new RotaryWithLabel();
        rotary->setLookAndFeel(&gModernRotaryLookAndFeel);
        rotary->setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.92f));
        rotaryKnobs_.add(rotary);
        addAndMakeVisible(rotary);

        auto button = new ToggleButton();
        button->setClickingTogglesState(true);
        button->setButtonText("Button " + juce::String(slotIndex + 1));
        button->onClick = [this, slotIndex]() { handlePressSlotClick(slotIndex); };
        buttonControls_.add(button);
        addAndMakeVisible(button);

        auto& slot = slots_[slotIndex];
        slot.type = ControllerType::Rotary;
        slot.rotary = rotary;
        slot.button = button;
        slot.buttonDefaultText = button->getButtonText();
        resetButtonSlotState(slotIndex);
    }

    initialiseControllerSlots();
    updateAssignmentHighlight();

    LambdaButtonStrip::TButtonMap buttons = {
        { "loadAssignments", { "Load layout", [this]() { handleLoadAssignmentsRequested(); } } },
        { "saveAssignments", { "Save layout", [this]() { handleSaveAssignmentsRequested(); } } },
    };
    buttons_ = std::make_unique<LambdaButtonStrip>(505, LambdaButtonStrip::Direction::Horizontal);
    buttons_->setButtonDefinitions(buttons);
    addAndMakeVisible(*buttons_);

    loadAssignmentsFromDisk();

    UIModel::instance()->currentSynth_.addChangeListener(this);
    UIModel::instance()->currentPatch_.addChangeListener(this);
    UIModel::instance()->currentPatchValues_.addChangeListener(this);
}

EditorView::~EditorView()
{
    for (auto* rotary : rotaryKnobs_)
        if (rotary != nullptr)
            rotary->setLookAndFeel(nullptr);
    flushAssignmentsIfDirty();
    UIModel::instance()->currentPatchValues_.removeChangeListener(this);
    UIModel::instance()->currentPatch_.removeChangeListener(this);
    UIModel::instance()->currentSynth_.removeChangeListener(this);
}

//==================================================================================================
// Layout & painting
//==================================================================================================

void EditorView::resized()
{
    auto bounds = getLocalBounds();

    const int paletteHeight = LAYOUT_LARGE_LINE_SPACING * 2;
    auto paletteArea = bounds.removeFromTop(paletteHeight);
    if (paletteContainer_ != nullptr)
    {
        paletteContainer_->setBounds(paletteArea.reduced(LAYOUT_INSET_NORMAL));
        auto paletteBounds = paletteContainer_->getLocalBounds();
        int itemWidth = juce::jmax(110, paletteBounds.getWidth() / juce::jmax<int>(1, (int)controllerPaletteItems_.size()));
        for (auto& item : controllerPaletteItems_)
        {
            item->setBounds(paletteBounds.removeFromLeft(itemWidth).reduced(LAYOUT_INSET_SMALL));
        }
    }

    auto buttonsArea = bounds.removeFromBottom(LAYOUT_LARGE_LINE_SPACING * 2);
    if (buttons_)
        buttons_->setBounds(buttonsArea.reduced(LAYOUT_INSET_NORMAL));

    const int sideWidth = juce::roundToInt(bounds.getWidth() * 0.18f);
    auto leftPanel = bounds.removeFromLeft(sideWidth);
    auto rightPanel = bounds.removeFromRight(sideWidth);

    valueTreeViewer_.setBounds(leftPanel.reduced(LAYOUT_INSET_NORMAL));
    patchTextBox_.setBounds(rightPanel.reduced(LAYOUT_INSET_NORMAL));

    auto gridArea = bounds.reduced(LAYOUT_INSET_NORMAL);
    const float cellWidth = gridArea.getWidth() / (float)gridCols_;
    const float cellHeight = gridArea.getHeight() / (float)gridRows_;

    for (int row = 0; row < gridRows_; ++row)
    {
        for (int col = 0; col < gridCols_; ++col)
        {
            int slotIndex = row * gridCols_ + col;
            if (slotIndex >= totalSlots_)
                continue;

            juce::Rectangle<float> cell(gridArea.getX() + col * cellWidth,
                                        gridArea.getY() + row * cellHeight,
                                        cellWidth,
                                        cellHeight);
            auto cellBounds = cell.toNearestInt().reduced(LAYOUT_INSET_SMALL);
            auto& slot = slots_[slotIndex];
            if (slot.rotary != nullptr)
                slot.rotary->setBounds(cellBounds);
            if (slot.button != nullptr)
                slot.button->setBounds(cellBounds);
        }
    }
}

void EditorView::paintOverChildren(juce::Graphics& g)
{
    if (hoveredSlotIndex_ < 0 || hoveredSlotIndex_ >= totalSlots_)
        return;

    auto& slot = slots_[hoveredSlotIndex_];
    juce::Component* component = slot.type == ControllerType::Rotary ? static_cast<juce::Component*>(slot.rotary)
                                                                     : static_cast<juce::Component*>(slot.button);
    if (component == nullptr)
        return;

    auto bounds = component->getBounds().toFloat().reduced(2.0f);
    g.setColour(kAccentColour.withAlpha(0.18f));
    g.fillRoundedRectangle(bounds, kCornerRadius);
    g.setColour(kAccentColour.withAlpha(0.75f));
    g.drawRoundedRectangle(bounds, kCornerRadius, 1.8f);
}

//==================================================================================================
// Drag & drop
//==================================================================================================

bool EditorView::isInterestedInDragSource(const juce::DragAndDropTarget::SourceDetails& details)
{
    bool isController = false;
    controllerTypeFromDescription(details.description, isController);
    if (isController)
        return true;

    return findParameterByName(details.description.toString()) != nullptr;
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
    auto localPos = mousePositionInLocalSpace();
    auto slotIndex = slotIndexAt(localPos);
    if (slotIndex < 0 || slotIndex >= totalSlots_)
        return;

    bool isController = false;
    auto controllerType = controllerTypeFromDescription(details.description, isController);
    if (isController)
    {
        handleControllerDrop(slotIndex, controllerType);
        clearDropHoverState();
        return;
    }

    auto parameter = findParameterByName(details.description.toString());
    if (!parameter)
        return;

    assignParameterToSlot(slotIndex, parameter, true);
    clearDropHoverState();
}

void EditorView::dragOperationEnded(const juce::DragAndDropTarget::SourceDetails& details)
{
    juce::DragAndDropContainer::dragOperationEnded(details);
    clearDropHoverState();
}

//==================================================================================================
// Synth change handling
//==================================================================================================

void EditorView::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    if (auto* current = dynamic_cast<CurrentSynth*>(source))
    {
        flushAssignmentsIfDirty();
        currentSynthName_.clear();
        currentLayoutNode_ = {};

        initialiseControllerSlots();

        if (auto synthPtr = current->smartSynth())
            currentSynthName_ = synthPtr->getName();

        auto supported = midikraft::Capability::hasCapability<midikraft::SynthParametersCapability>(current->smartSynth());
        if (supported)
        {
            uiModel_ = createParameterModel();

            uiValueTree_.removeListener(&updateSynthListener_);
            uiValueTree_ = juce::ValueTree("UIMODEL");
            uiModel_.addToValueTree(uiValueTree_);
            valueTreeViewer_.setValueTree(uiValueTree_);

            auto initPatch = std::dynamic_pointer_cast<midikraft::CreateInitPatchDataCapability>(current->smartSynth());
            if (initPatch)
            {
                auto newPatch = current->smartSynth()->patchFromPatchData(initPatch->createInitPatch(), MidiProgramNumber::fromZeroBase(0));
                updateSynthListener_.updateAllKnobsFromPatch(current->smartSynth(), newPatch);
            }
            else
            {
                updateSynthListener_.updateAllKnobsFromPatch(current->smartSynth(), nullptr);
            }

            uiValueTree_.addListener(&updateSynthListener_);
            loadAssignmentsForSynth(current->smartSynth());
        }
    }
    else if (dynamic_cast<CurrentPatch*>(source) || source == &UIModel::instance()->currentPatchValues_)
    {
        updateSynthListener_.updateAllKnobsFromPatch(UIModel::currentPatch().smartSynth(), UIModel::currentPatch().patch());
    }
}

//==================================================================================================
// Assignment helpers
//==================================================================================================

TypedNamedValueSet EditorView::createParameterModel()
{
    TypedNamedValueSet result;
    auto detailedParameters = midikraft::Capability::hasCapability<midikraft::SynthParametersCapability>(UIModel::currentSynthOfPatchSmart());
    if (detailedParameters)
    {
        for (auto param : detailedParameters->getParameterDefinitions())
        {
            auto tnv = makeTypedNamedValue(param);
            if (tnv)
            {
                tnv->value().setValue(tnv->maxValue() + 1); // force refresh
                result.push_back(tnv);
            }
        }
    }
    return result;
}

std::shared_ptr<TypedNamedValue> EditorView::findParameterByName(const juce::String& propertyName)
{
    auto trimmed = propertyName.trim();
    if (trimmed.isEmpty())
        return {};
    auto name = trimmed.toStdString();
    if (!uiModel_.hasValue(name))
        return {};
    return uiModel_.typedNamedValueByName(name);
}

void EditorView::assignParameterToSlot(int slotIndex, std::shared_ptr<TypedNamedValue> param, bool updateStorage)
{
    if (!param || slotIndex < 0 || slotIndex >= totalSlots_)
        return;

    auto& slot = slots_[slotIndex];
    auto newName = param->name().toStdString();

    if (slot.type == ControllerType::Button && !canAssignToPress(*param))
    {
        spdlog::warn("Parameter {} is not suitable for a button controller", newName);
        return;
    }

    replaceAssignmentName(slot.assignedParameter, newName);

    if (slot.type == ControllerType::Rotary)
    {
        slot.rotary->setSynthParameter(param.get());
        auto valueVar = param->value().getValue();
        double numericValue = 0.0;
        if (valueVar.isDouble())
            numericValue = static_cast<double>(valueVar);
        else if (valueVar.isInt() || valueVar.isInt64())
            numericValue = static_cast<double>(static_cast<int>(valueVar));
        else if (valueVar.isBool())
            numericValue = static_cast<bool>(valueVar) ? 1.0 : 0.0;
        else
            numericValue = static_cast<double>(param->minValue());
        slot.rotary->setValue(juce::roundToInt(numericValue));
    }
    else
    {
        auto& binding = slot.pressBinding;
        binding.listener.reset();
        binding.param = param;
        binding.usesBool = param->valueType() == ValueType::Bool;

        if (!binding.usesBool)
        {
            int offValue = 0;
            int onValue = 1;
            if (!extractBinaryValues(*param, offValue, onValue))
                return;
            binding.offValue = offValue;
            binding.onValue = onValue;
        }
        else
        {
            binding.offValue = 0;
            binding.onValue = 1;
        }

        binding.listener = std::make_unique<LambdaValueListener>(param->value(), [this, slotIndex](juce::Value&) {
            refreshPressButtonSlot(slotIndex);
        });

        refreshPressButtonSlot(slotIndex);
        slot.button->setTooltip("Controls " + param->name());
    }

    if (updateStorage && !loadingAssignments_)
    {
        storeSlotAssignment(slotIndex);
        markAssignmentsDirty();
    }
    updateAssignmentHighlight();
}

bool EditorView::canAssignToPress(const TypedNamedValue& param) const
{
    if (param.valueType() == ValueType::Bool)
        return true;
    int offValue = 0;
    int onValue = 0;
    return extractBinaryValues(param, offValue, onValue);
}

bool EditorView::extractBinaryValues(const TypedNamedValue& param, int& offValue, int& onValue) const
{
    switch (param.valueType())
    {
    case ValueType::Integer:
        if (param.maxValue() - param.minValue() == 1)
        {
            offValue = param.minValue();
            onValue = param.maxValue();
            return true;
        }
        break;
    case ValueType::Lookup:
    {
        auto lookup = param.lookup();
        if (lookup.size() == 2)
        {
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

void EditorView::refreshPressButtonSlot(int slotIndex)
{
    if (slotIndex < 0 || slotIndex >= totalSlots_)
        return;

    auto& slot = slots_[slotIndex];
    if (!slot.button || !slot.pressBinding.param)
        return;

    auto valueVar = slot.pressBinding.param->value().getValue();
    bool isOn = slot.pressBinding.usesBool ? static_cast<bool>(valueVar)
                                           : (static_cast<int>(valueVar) == slot.pressBinding.onValue);
    slot.button->setToggleState(isOn, juce::dontSendNotification);
    slot.button->setButtonText(slot.pressBinding.param->name() + ": " + buttonValueText(*slot.pressBinding.param, valueVar));
}

void EditorView::handlePressSlotClick(int slotIndex)
{
    if (slotIndex < 0 || slotIndex >= totalSlots_)
        return;

    auto& slot = slots_[slotIndex];
    auto& binding = slot.pressBinding;
    if (!binding.param)
    {
        slot.button->setToggleState(false, juce::dontSendNotification);
        return;
    }

    bool shouldBeOn = slot.button->getToggleState();
    if (binding.usesBool)
        binding.param->value().setValue(shouldBeOn);
    else
        binding.param->value().setValue(shouldBeOn ? binding.onValue : binding.offValue);
}

juce::String EditorView::buttonValueText(const TypedNamedValue& param, const juce::var& value) const
{
    switch (param.valueType())
    {
    case ValueType::Bool:
        return (bool)value ? "On" : "Off";
    case ValueType::Lookup:
    {
        auto lookup = param.lookup();
        auto intValue = static_cast<int>(value);
        auto found = lookup.find(intValue);
        if (found != lookup.end())
            return found->second;
        break;
    }
    default:
        break;
    }

    if (value.isString())
        return value.toString();
    if (value.isInt() || value.isDouble())
        return juce::String(static_cast<int>(value));
    return value.toString();
}

void EditorView::setEditorPatch(std::shared_ptr<midikraft::Synth> synth, std::shared_ptr<midikraft::DataFile> data)
{
    if (synth && data)
        editorPatchHolder_ = std::make_shared<midikraft::PatchHolder>(synth, nullptr, data);
    else
        editorPatchHolder_.reset();
    patchTextBox_.fillTextBox(editorPatchHolder_);
}

void EditorView::refreshEditorPatch()
{
    if (editorPatchHolder_)
        patchTextBox_.fillTextBox(editorPatchHolder_);
}

//==================================================================================================
// Assignment persistence
//==================================================================================================

void EditorView::loadAssignmentsForSynth(std::shared_ptr<midikraft::Synth> synth)
{
    currentLayoutNode_ = {};
    if (!synth)
        return;

    if (!assignmentsLoaded_)
        loadAssignmentsFromDisk();

    initialiseControllerSlots();

    auto synthName = juce::String(synth->getName());
    currentLayoutNode_ = findLayoutNode(synthName);
    if (!currentLayoutNode_.isValid())
    {
        currentLayoutNode_ = ensureLayoutNode(synthName);
        updateAssignmentHighlight();
        return;
    }

    loadingAssignments_ = true;

    auto slotsNode = currentLayoutNode_.getChildWithName(kSlotsNodeId);
    if (slotsNode.isValid())
    {
        for (int i = 0; i < slotsNode.getNumChildren(); ++i)
        {
            applyAssignmentToSlotFromTree(slotsNode.getChild(i));
        }
    }

    loadingAssignments_ = false;
    updateAssignmentHighlight();
}

void EditorView::applyAssignmentsToCurrentSynth()
{
    initialiseControllerSlots();

    if (!currentLayoutNode_.isValid())
    {
        updateAssignmentHighlight();
        return;
    }

    loadingAssignments_ = true;

    auto slotsNode = currentLayoutNode_.getChildWithName(kSlotsNodeId);
    if (slotsNode.isValid())
    {
        for (int i = 0; i < slotsNode.getNumChildren(); ++i)
            applyAssignmentToSlotFromTree(slotsNode.getChild(i));
    }

    loadingAssignments_ = false;
    updateAssignmentHighlight();
}

void EditorView::applyAssignmentsFromTree(const juce::ValueTree& layoutTree)
{
    initialiseControllerSlots();
    auto slotsNode = layoutTree.getChildWithName(kSlotsNodeId);
    if (!slotsNode.isValid())
        return;

    loadingAssignments_ = true;
    for (int i = 0; i < slotsNode.getNumChildren(); ++i)
        applyAssignmentToSlotFromTree(slotsNode.getChild(i));
    loadingAssignments_ = false;
    updateAssignmentHighlight();
}

void EditorView::applyAssignmentToSlotFromTree(const juce::ValueTree& assignmentNode)
{
    if (!assignmentNode.hasType(kSlotNodeId))
        return;

    if (!assignmentNode.hasProperty(kIndexProperty))
        return;

    int slotIndex = (int)assignmentNode.getProperty(kIndexProperty);
    if (slotIndex < 0 || slotIndex >= totalSlots_)
        return;

    auto controllerType = ControllerType::Rotary;
    if (assignmentNode.hasProperty(kControllerProperty))
        controllerType = stringToControllerType(assignmentNode.getProperty(kControllerProperty).toString());

    setSlotType(slotIndex, controllerType, false);

    if (assignmentNode.hasProperty(kParameterProperty))
    {
        auto parameterName = assignmentNode.getProperty(kParameterProperty).toString().toStdString();
        if (!parameterName.empty() && uiModel_.hasValue(parameterName))
        {
            assignParameterToSlot(slotIndex, uiModel_.typedNamedValueByName(parameterName), false);
        }
    }
}

void EditorView::storeSlotAssignment(int slotIndex)
{
    if (currentSynthName_.isEmpty() || slotIndex < 0 || slotIndex >= totalSlots_)
        return;

    auto layoutNode = ensureLayoutNode(currentSynthName_);
    auto slotsNode = ensureSection(layoutNode, kSlotsNodeId);
    auto assignment = ensureAssignmentNode(slotsNode, slotIndex);

    assignment.setProperty(kControllerProperty, controllerTypeToString(slots_[slotIndex].type), nullptr);
    if (slots_[slotIndex].assignedParameter.empty())
        assignment.removeProperty(kParameterProperty, nullptr);
    else
        assignment.setProperty(kParameterProperty, String(slots_[slotIndex].assignedParameter), nullptr);
}

juce::ValueTree EditorView::ensureLayoutNode(const juce::String& synthName)
{
    if (!assignmentsRoot_.isValid())
        assignmentsRoot_ = juce::ValueTree(kAssignmentsRootId);

    auto synthNode = assignmentsRoot_.getChildWithProperty(kSynthNameProperty, synthName);
    if (!synthNode.isValid())
    {
        synthNode = juce::ValueTree(kSynthNodeId);
        synthNode.setProperty(kSynthNameProperty, synthName, nullptr);
        assignmentsRoot_.addChild(synthNode, -1, nullptr);
    }

    auto layoutNode = synthNode.getChildWithProperty(kLayoutIdProperty, currentLayoutId_);
    if (!layoutNode.isValid())
    {
        layoutNode = juce::ValueTree(kLayoutNodeId);
        layoutNode.setProperty(kLayoutIdProperty, currentLayoutId_, nullptr);
        synthNode.addChild(layoutNode, -1, nullptr);
    }

    return layoutNode;
}

juce::ValueTree EditorView::findLayoutNode(const juce::String& synthName) const
{
    if (!assignmentsRoot_.isValid())
        return {};

    auto synthNode = assignmentsRoot_.getChildWithProperty(kSynthNameProperty, synthName);
    if (!synthNode.isValid())
        return {};
    return synthNode.getChildWithProperty(kLayoutIdProperty, currentLayoutId_);
}

juce::ValueTree EditorView::ensureSection(juce::ValueTree parent, const juce::Identifier& sectionId)
{
    if (!parent.isValid())
        return {};

    auto section = parent.getChildWithName(sectionId);
    if (!section.isValid())
    {
        section = juce::ValueTree(sectionId);
        parent.addChild(section, -1, nullptr);
    }
    return section;
}

juce::ValueTree EditorView::findAssignmentNode(juce::ValueTree parent, int index) const
{
    if (!parent.isValid())
        return {};

    for (int i = 0; i < parent.getNumChildren(); ++i)
    {
        auto child = parent.getChild(i);
        if (child.hasProperty(kIndexProperty) && (int)child.getProperty(kIndexProperty) == index)
            return child;
    }
    return {};
}

juce::ValueTree EditorView::ensureAssignmentNode(juce::ValueTree parent, int index)
{
    auto node = findAssignmentNode(parent, index);
    if (!node.isValid())
    {
        node = juce::ValueTree(kSlotNodeId);
        node.setProperty(kIndexProperty, index, nullptr);
        parent.addChild(node, -1, nullptr);
    }
    return node;
}

void EditorView::loadAssignmentsFromDisk()
{
    auto file = assignmentsFile();
    assignmentsRoot_ = juce::ValueTree(kAssignmentsRootId);

    if (file.existsAsFile())
    {
        juce::FileInputStream stream(file);
        if (stream.openedOk())
        {
            auto tree = juce::ValueTree::readFromStream(stream);
            if (tree.isValid() && tree.getType() == kAssignmentsRootId)
                assignmentsRoot_ = tree;
        }
    }

    assignmentsLoaded_ = true;
    assignmentsDirty_ = false;
}

void EditorView::saveAssignmentsToDisk()
{
    if (!assignmentsRoot_.isValid())
        return;

    auto file = assignmentsFile();
    auto directory = file.getParentDirectory();
    if (!directory.exists())
        directory.createDirectory();

    juce::TemporaryFile temp(file);
    {
        juce::FileOutputStream out(temp.getFile());
        if (!out.openedOk())
        {
            spdlog::error("Failed to open temporary file for writing assignments {}", temp.getFile().getFullPathName().toStdString());
            return;
        }
        assignmentsRoot_.writeToStream(out);
        out.flush();
        if (out.getStatus().failed())
        {
            spdlog::error("Failed while writing assignments file {}: {}", temp.getFile().getFullPathName().toStdString(), out.getStatus().getErrorMessage().toStdString());
            return;
        }
    }

    if (!temp.overwriteTargetFileWithTemporary())
    {
        spdlog::error("Failed to overwrite assignments file {}", file.getFullPathName().toStdString());
        return;
    }

    assignmentsDirty_ = false;
    spdlog::info("Controller assignments saved to {}", file.getFullPathName().toStdString());
}

void EditorView::handleLoadAssignmentsRequested()
{
    if (assignmentsDirty_)
        spdlog::info("Discarding unsaved controller assignments before loading from disk");

    loadAssignmentsFromDisk();

    if (currentSynthName_.isEmpty())
        return;

    currentLayoutNode_ = findLayoutNode(currentSynthName_);
    applyAssignmentsToCurrentSynth();
}

void EditorView::handleSaveAssignmentsRequested()
{
    if (!assignmentsLoaded_)
        loadAssignmentsFromDisk();

    if (assignmentsDirty_)
        saveAssignmentsToDisk();
    else
        spdlog::info("Controller assignments unchanged, nothing to save");
}

juce::File EditorView::assignmentsFile() const
{
    auto& settingsFile = Settings::instance().getPropertiesFile();
    auto directory = settingsFile.getParentDirectory();
    return directory.getChildFile("KnobAssignments.xml");
}

void EditorView::markAssignmentsDirty()
{
    if (!loadingAssignments_)
        assignmentsDirty_ = true;
}

void EditorView::flushAssignmentsIfDirty()
{
    if (assignmentsDirty_)
        saveAssignmentsToDisk();
}

//==================================================================================================
// Slot helpers
//==================================================================================================

void EditorView::resetButtonSlotState(int slotIndex)
{
    if (slotIndex < 0 || slotIndex >= totalSlots_)
        return;

    auto& slot = slots_[slotIndex];
    if (slot.pressBinding.listener)
        slot.pressBinding.listener.reset();
    slot.pressBinding.param.reset();
    slot.pressBinding.usesBool = false;
    slot.pressBinding.offValue = 0;
    slot.pressBinding.onValue = 1;

    if (slot.button != nullptr)
    {
        slot.button->setToggleState(false, juce::dontSendNotification);
        slot.button->setButtonText(slot.buttonDefaultText);
        slot.button->setTooltip({});
    }
}

void EditorView::updateAssignmentHighlight()
{
    if (valueTreeViewer_.getValueTree().isValid())
        valueTreeViewer_.refresh();
}

void EditorView::incrementAssignment(const std::string& name)
{
    if (name.empty())
        return;
    assignmentUsage_[name] += 1;
}

void EditorView::decrementAssignment(const std::string& name)
{
    if (name.empty())
        return;
    auto it = assignmentUsage_.find(name);
    if (it != assignmentUsage_.end())
    {
        if (--it->second <= 0)
            assignmentUsage_.erase(it);
    }
}

void EditorView::replaceAssignmentName(std::string& slotName, const std::string& newName)
{
    if (slotName == newName)
        return;
    if (!slotName.empty())
        decrementAssignment(slotName);
    slotName = newName;
    if (!slotName.empty())
        incrementAssignment(slotName);
}

void EditorView::initialiseControllerSlots()
{
    assignmentUsage_.clear();
    for (int i = 0; i < totalSlots_; ++i)
    {
        slots_[i].type = ControllerType::Rotary;
        replaceAssignmentName(slots_[i].assignedParameter, "");
        resetButtonSlotState(i);
        updateSlotVisibility(i);
    }
}

void EditorView::updateSlotVisibility(int slotIndex)
{
    if (slotIndex < 0 || slotIndex >= totalSlots_)
        return;

    auto& slot = slots_[slotIndex];
    if (slot.rotary != nullptr)
    {
        slot.rotary->setVisible(slot.type == ControllerType::Rotary);
        if (slot.type == ControllerType::Button)
            slot.rotary->setUnused();
    }
    if (slot.button != nullptr)
        slot.button->setVisible(slot.type == ControllerType::Button);
}

void EditorView::setSlotType(int slotIndex, ControllerType type, bool recordChange)
{
    if (slotIndex < 0 || slotIndex >= totalSlots_)
        return;

    auto& slot = slots_[slotIndex];
    if (slot.type == type)
        return;

    if (!slot.assignedParameter.empty())
        replaceAssignmentName(slot.assignedParameter, "");

    if (slot.type == ControllerType::Button)
        resetButtonSlotState(slotIndex);

    slot.type = type;
    if (slot.type == ControllerType::Button && slot.assignedParameter.empty())
        resetButtonSlotState(slotIndex);
    updateSlotVisibility(slotIndex);

    if (recordChange && !loadingAssignments_)
    {
        storeSlotAssignment(slotIndex);
        markAssignmentsDirty();
        updateAssignmentHighlight();
    }
}

int EditorView::slotIndexForComponent(juce::Component* component) const
{
    for (int i = 0; i < totalSlots_; ++i)
    {
        if (slots_[i].rotary == component || slots_[i].button == component)
            return i;
    }
    return -1;
}

int EditorView::slotIndexFromRotaryIndex(int rotaryIndex) const
{
    if (rotaryIndex < 0 || rotaryIndex >= rotaryKnobs_.size())
        return -1;
    auto* rotary = rotaryKnobs_[rotaryIndex];
    return slotIndexForComponent(rotary);
}

int EditorView::slotIndexFromButtonIndex(int buttonIndex) const
{
    if (buttonIndex < 0 || buttonIndex >= buttonControls_.size())
        return -1;
    auto* button = buttonControls_[buttonIndex];
    return slotIndexForComponent(button);
}

int EditorView::slotIndexAt(juce::Point<int> localPos) const
{
    for (int i = 0; i < totalSlots_; ++i)
    {
        auto const& slot = slots_[i];
        juce::Component* component = slot.type == ControllerType::Rotary ? static_cast<juce::Component*>(slot.rotary)
                                                                        : static_cast<juce::Component*>(slot.button);
        if (component != nullptr && component->isShowing() && component->getBounds().contains(localPos))
            return i;
    }
    return -1;
}

void EditorView::handleControllerDrop(int slotIndex, ControllerType type)
{
    if (slotIndex < 0 || slotIndex >= totalSlots_)
        return;

    setSlotType(slotIndex, type, true);
    replaceAssignmentName(slots_[slotIndex].assignedParameter, "");
    resetButtonSlotState(slotIndex);
    markAssignmentsDirty();
    updateAssignmentHighlight();
}

EditorView::ControllerType EditorView::controllerTypeFromDescription(const juce::var& description, bool& isController) const
{
    isController = false;
    if (!description.isString())
        return ControllerType::Rotary;
    auto text = description.toString();
    if (text.compareIgnoreCase("controller:rotary") == 0)
    {
        isController = true;
        return ControllerType::Rotary;
    }
    if (text.compareIgnoreCase("controller:button") == 0)
    {
        isController = true;
        return ControllerType::Button;
    }
    return ControllerType::Rotary;
}

juce::Point<int> EditorView::mousePositionInLocalSpace() const
{
    auto screenPos = juce::Desktop::getInstance().getMainMouseSource().getScreenPosition();
    return getLocalPoint(nullptr, screenPos.roundToInt());
}

void EditorView::updateDropHoverState(const juce::DragAndDropTarget::SourceDetails& details)
{
    bool isController = false;
    auto localPos = mousePositionInLocalSpace();
    auto slotIndex = slotIndexAt(localPos);

    bool canDrop = false;

    if (slotIndex >= 0)
    {
        if (isController)
        {
            canDrop = true;
        }
        else
        {
            auto parameter = findParameterByName(details.description.toString());
            if (parameter)
            {
                auto targetType = slots_[slotIndex].type;
                if (targetType == ControllerType::Button)
                    canDrop = canAssignToPress(*parameter);
                else
                    canDrop = true;
            }
        }
    }

    if (!canDrop)
        slotIndex = -1;

    if (slotIndex != hoveredSlotIndex_)
    {
        hoveredSlotIndex_ = slotIndex;
        repaint();
    }

    setMouseCursor(canDrop ? juce::MouseCursor::CopyingCursor : juce::MouseCursor::NormalCursor);
}

void EditorView::clearDropHoverState()
{
    bool hadHover = hoveredSlotIndex_ != -1;
    hoveredSlotIndex_ = -1;
    setMouseCursor(juce::MouseCursor::NormalCursor);
    if (hadHover)
        repaint();
}

//==================================================================================================
// BCR2000Proxy implementation (legacy API)
//==================================================================================================

void EditorView::setRotaryParam(int knobNumber, TypedNamedValue* param)
{
    if (!param)
        return;
    int rotaryIndex = knobNumber - 1;
    auto slotIndex = slotIndexFromRotaryIndex(rotaryIndex);
    if (slotIndex < 0)
        return;
    setSlotType(slotIndex, ControllerType::Rotary, false);
    auto paramName = param->name().toStdString();
    auto shared = uiModel_.typedNamedValueByName(paramName);
    if (!shared)
        shared = std::shared_ptr<TypedNamedValue>(param, [](TypedNamedValue*) {});
    assignParameterToSlot(slotIndex, shared, true);
}

void EditorView::setButtonParam(int knobNumber, std::string const& name)
{
    int buttonIndex = knobNumber - 1;
    auto slotIndex = slotIndexFromButtonIndex(buttonIndex);
    if (slotIndex < 0)
        return;
    setSlotType(slotIndex, ControllerType::Button, true);
    if (slots_[slotIndex].button != nullptr)
    {
        slots_[slotIndex].buttonDefaultText = juce::String(name);
        slots_[slotIndex].button->setButtonText(slots_[slotIndex].buttonDefaultText);
    }
}

//==================================================================================================
// Button handlers
//==================================================================================================

//==================================================================================================
// UpdateSynthListener implementation
//==================================================================================================

EditorView::UpdateSynthListener::UpdateSynthListener(EditorView* parent)
    : owner_(parent)
{
    editBuffer_ = std::make_shared<midikraft::DataFile>(0);
    midikraft::MidiController::instance()->addMessageHandler(midiHandler_, [this](juce::MidiInput* source, juce::MidiMessage const& message) {
        listenForMidiMessages(source, message);
    });
}

EditorView::UpdateSynthListener::~UpdateSynthListener()
{
    midikraft::MidiController::instance()->removeMessageHandler(midiHandler_);
}

void EditorView::UpdateSynthListener::valueTreePropertyChanged(juce::ValueTree& treeWhosePropertyHasChanged, const juce::Identifier& property)
{
    auto detailedParameters = midikraft::Capability::hasCapability<midikraft::SynthParametersCapability>(UIModel::currentSynthOfPatchSmart());
    if (!detailedParameters)
        return;

    std::string paramName = property.toString().toStdString();
    bool found = false;
    for (auto param : detailedParameters->getParameterDefinitions())
    {
        if (param.name == paramName)
        {
            midikraft::ParamVal newValue{ param.param_id, treeWhosePropertyHasChanged.getProperty(property) };
            detailedParameters->setParameterValues(editBuffer_, { newValue });

            auto location = std::dynamic_pointer_cast<midikraft::SimpleDiscoverableDevice>(UIModel::currentSynthOfPatchSmart());
            auto messages = detailedParameters->createSetValueMessages(location ? location->channel() : MidiChannel::invalidChannel(), editBuffer_, { param.param_id });
            if (!messages.empty())
            {
                if (location && location->wasDetected())
                    UIModel::currentSynthOfPatch()->sendBlockOfMessagesToSynth(location->midiOutput(), messages);
                else
                    spdlog::info("Synth not detected, can't send message to update {}", param.name);
            }
            owner_->refreshEditorPatch();
            found = true;
            break;
        }
    }
    if (!found)
        spdlog::error("Failed to find parameter definition for property {}", property.toString().toStdString());
}

void EditorView::UpdateSynthListener::listenForMidiMessages(juce::MidiInput* source, juce::MidiMessage message)
{
    auto synth = UIModel::currentSynthOfPatch();
    auto location = dynamic_cast<midikraft::MidiLocationCapability*>(synth);
    if (!location || location->midiInput().name == source->getName())
    {
        auto syncCap = dynamic_cast<midikraft::BidirectionalSyncCapability*>(synth);
        if (syncCap)
        {
            int outValue;
            std::shared_ptr<midikraft::SynthParameterDefinition> param;
            if (syncCap->determineParameterChangeFromSysex({ message }, &param, outValue))
            {
                owner_->uiValueTree_.setPropertyExcludingListener(this, juce::Identifier(param->name()), outValue, nullptr);
            }
        }

        if (message.isProgramChange() && (!location || location->channel().toOneBasedInt() == message.getChannel()))
        {
            auto programChangeCap = dynamic_cast<midikraft::SendsProgramChangeCapability*>(synth);
            if (programChangeCap)
            {
                programChangeCap->gotProgramChange(MidiProgramNumber::fromZeroBase(message.getProgramChangeNumber()));
                if (location)
                {
                    owner_->librarian_.downloadEditBuffer(midikraft::MidiController::instance()->getMidiOutput(location->midiOutput()),
                                                          UIModel::currentSynthOfPatchSmart(), nullptr,
                                                          [this](std::vector<midikraft::PatchHolder> patch) {
                                                              if (!patch.empty() && patch[0].patch())
                                                                  updateAllKnobsFromPatch(patch[0].smartSynth(), patch[0].patch());
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
    if (!detailedParameters)
        return;

    if (newPatch)
    {
        editBuffer_->setData(newPatch->data());
        auto values = detailedParameters->getParameterValues(editBuffer_, false);
        for (auto param : detailedParameters->getParameterDefinitions())
        {
            auto value = valueForParameter(param, values);
            if (value.has_value())
            {
                switch (param.param_type)
                {
                case midikraft::ParamType::VALUE:
                    break;
                case midikraft::ParamType::CHOICE:
                {
                    auto valueArray = param.values.getArray();
                    juce::var clearTextValue = value->value;
                    value.reset();
                    if (valueArray)
                    {
                        int index = 0;
                        for (auto elementPtr = valueArray->begin(); elementPtr != valueArray->end(); ++elementPtr, ++index)
                        {
                            if (*elementPtr == clearTextValue)
                            {
                                value = midikraft::ParamVal({ param.param_id, juce::var(index) });
                                break;
                            }
                        }
                    }
                    break;
                }
                default:
                    spdlog::warn("parameter type not yet implemented for parameter {}", param.name);
                    break;
                }
            }

            if (value.has_value() && owner_->uiValueTree_.hasProperty(juce::Identifier(param.name)))
            {
                owner_->uiValueTree_.setPropertyExcludingListener(this, juce::Identifier(param.name), (int)value->value, nullptr);
            }
        }
    }
    else
    {
        for (auto param : detailedParameters->getParameterDefinitions())
        {
            if (owner_->uiValueTree_.hasProperty(juce::Identifier(param.name)))
            {
                switch (param.param_type)
                {
                case midikraft::ParamType::VALUE:
                    owner_->uiValueTree_.setPropertyExcludingListener(this, juce::Identifier(param.name), (int)(param.values[0]), nullptr);
                    break;
                case midikraft::ParamType::CHOICE:
                    owner_->uiValueTree_.setPropertyExcludingListener(this, juce::Identifier(param.name), 0, nullptr);
                    break;
                default:
                    break;
                }
            }
        }
        owner_->setEditorPatch(nullptr, nullptr);
        return;
    }
    owner_->setEditorPatch(synth, editBuffer_);
}

//==================================================================================================
// Legacy helpers from original implementation
//==================================================================================================
