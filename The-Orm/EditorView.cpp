/*
   Copyright (c) 2020-2025 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "EditorView.h"

#include "EnvelopeControl.h"
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
#include <memory>
#include <limits>
#include <vector>
#include <array>
#include <algorithm>

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
const juce::Identifier kControllerVariantProperty("controllerVariant");

struct EnvelopeVariantSpec
{
    juce::String id;
    juce::String paletteLabel;
    juce::String description;
    std::vector<EnvelopeControl::StageSpecification> stages;
};

using StageSpec = EnvelopeControl::StageSpecification;
using StageRole = StageSpec::Role;
using StageTarget = StageSpec::TargetType;

StageSpec makeTimeStage(const char* id,
                        const char* shortName,
                        const char* displayName,
                        double defaultWeight,
                        StageTarget targetType,
                        double absoluteLevel = 0.0,
                        const char* targetStageId = "")
{
    StageSpec spec;
    spec.id = id;
    spec.shortName = shortName;
    spec.displayName = displayName;
    spec.defaultNormalisedValue = defaultWeight;
    spec.role = StageRole::Time;
    spec.targetType = targetType;
    spec.absoluteLevel = absoluteLevel;
    spec.targetStageId = targetStageId;
    return spec;
}

StageSpec makeLevelStage(const char* id, const char* shortName, const char* displayName, double defaultLevel)
{
    StageSpec spec;
    spec.id = id;
    spec.shortName = shortName;
    spec.displayName = displayName;
    spec.defaultNormalisedValue = defaultLevel;
    spec.role = StageRole::Level;
    spec.targetType = StageTarget::Hold;
    spec.absoluteLevel = defaultLevel;
    return spec;
}

const std::vector<EnvelopeVariantSpec>& envelopeVariants()
{
    static const std::vector<EnvelopeVariantSpec> variants = {
        { "adsr",
          "ADSR",
          "Standard Attack/Decay/Sustain/Release",
          {
              makeTimeStage("attack", "A", "Attack", 0.35, StageTarget::Absolute, 1.0),
              makeTimeStage("decay", "D", "Decay", 0.45, StageTarget::Stage, 0.0, "sustain"),
              makeLevelStage("sustain", "S", "Sustain", 0.60),
              makeTimeStage("release", "R", "Release", 0.40, StageTarget::Absolute, 0.0),
          } },
        { "dadsr",
          "Delay + ADSR",
          "Delay plus Attack/Decay/Sustain/Release",
          {
              makeTimeStage("delay", "D", "Delay", 0.25, StageTarget::Hold),
              makeTimeStage("attack", "A", "Attack", 0.35, StageTarget::Absolute, 1.0),
              makeTimeStage("decay", "D", "Decay", 0.45, StageTarget::Stage, 0.0, "sustain"),
              makeLevelStage("sustain", "S", "Sustain", 0.60),
              makeTimeStage("release", "R", "Release", 0.40, StageTarget::Absolute, 0.0),
          } },
        { "adsbslr",
          "ADSR + Break/Slope",
          "Attack/Decay with Breakpoint and Slope",
          {
              makeTimeStage("attack", "A", "Attack", 0.30, StageTarget::Absolute, 1.0),
              makeTimeStage("decay", "D", "Decay", 0.40, StageTarget::Stage, 0.0, "breakpoint"),
              makeLevelStage("breakpoint", "B", "Breakpoint", 0.55),
              makeTimeStage("slope", "SL", "Slope", 0.35, StageTarget::Stage, 0.0, "sustain"),
              makeLevelStage("sustain", "S", "Sustain", 0.60),
              makeTimeStage("release", "R", "Release", 0.40, StageTarget::Absolute, 0.0),
          } },
        { "yamaha4pt",
          "Yamaha 4-Point",
          "Four points with time and level parameters",
          {
              makeTimeStage("p1x", "P1X", "Point 1 Time", 0.20, StageTarget::Stage, 0.0, "p1y"),
              makeLevelStage("p1y", "P1Y", "Point 1 Level", 0.80),
              makeTimeStage("p2x", "P2X", "Point 2 Time", 0.25, StageTarget::Stage, 0.0, "p2y"),
              makeLevelStage("p2y", "P2Y", "Point 2 Level", 0.70),
              makeTimeStage("p3x", "P3X", "Point 3 Time", 0.30, StageTarget::Stage, 0.0, "p3y"),
              makeLevelStage("p3y", "P3Y", "Point 3 Level", 0.55),
              makeTimeStage("p4x", "P4X", "Point 4 Time", 0.25, StageTarget::Stage, 0.0, "p4y"),
              makeLevelStage("p4y", "P4Y", "Point 4 Level", 0.35),
          } },
    };
    return variants;
}

const EnvelopeVariantSpec& defaultEnvelopeVariant()
{
    return envelopeVariants().front();
}

const EnvelopeVariantSpec& envelopeVariantById(const juce::String& id)
{
    auto& variants = envelopeVariants();
    for (auto const& variant : variants)
        if (variant.id.equalsIgnoreCase(id))
            return variant;
    return defaultEnvelopeVariant();
}

juce::Identifier envelopeStagePropertyId(int stageIndex)
{
    return juce::Identifier("envelopeStage" + juce::String(stageIndex));
}

const std::array<juce::Identifier, 4> kLegacyEnvelopeStageProperties = {
    juce::Identifier("attackParameter"),
    juce::Identifier("decayParameter"),
    juce::Identifier("sustainParameter"),
    juce::Identifier("releaseParameter")
};

constexpr int kEnvelopeRowSpan = 2;
constexpr int kEnvelopeColSpan = 3;

EnvelopeControl::Specification makeSpecificationFromVariant(const EnvelopeVariantSpec& variant)
{
    EnvelopeControl::Specification specification;
    specification.id = variant.id;
    specification.displayName = variant.description;
    specification.stages = variant.stages;
    return specification;
}


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
    switch (type)
    {
    case EditorView::ControllerType::Empty:
        return "empty";
    case EditorView::ControllerType::Rotary:
        return "rotary";
    case EditorView::ControllerType::Button:
        return "button";
    case EditorView::ControllerType::Dropdown:
        return "dropdown";
    case EditorView::ControllerType::Envelope:
        return "envelope";
    }
    jassertfalse;
    return "empty";
}

EditorView::ControllerType stringToControllerType(const juce::String& str)
{
    if (str.compareIgnoreCase("button") == 0)
        return EditorView::ControllerType::Button;
    if (str.compareIgnoreCase("empty") == 0)
        return EditorView::ControllerType::Empty;
    if (str.compareIgnoreCase("dropdown") == 0)
        return EditorView::ControllerType::Dropdown;
    if (str.compareIgnoreCase("envelope") == 0 || str.compareIgnoreCase("adsr") == 0)
        return EditorView::ControllerType::Envelope;
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

EditorView::ControllerPaletteItem::ControllerPaletteItem(EditorView& owner,
                                                         ControllerType type,
                                                         juce::String const& labelText,
                                                         juce::String const& variantId)
    : owner_(owner), type_(type), label_(labelText), variantId_(variantId)
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
        juce::String descriptionText;
        switch (type_)
        {
        case ControllerType::Empty:
            descriptionText = "controller:empty";
            break;
        case ControllerType::Rotary:
            descriptionText = "controller:rotary";
            break;
        case ControllerType::Button:
            descriptionText = "controller:button";
            break;
        case ControllerType::Dropdown:
            descriptionText = "controller:dropdown";
            break;
        case ControllerType::Envelope:
            descriptionText = "controller:envelope";
            if (!variantId_.isEmpty())
                descriptionText += ":" + variantId_;
            break;
        }
        const juce::var description(descriptionText);
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

void EditorView::ControllerPaletteItem::setTooltipText(const juce::String& text)
{
    tooltip_ = text;
    setTooltip(tooltip_);
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
    controllerPaletteItems_.push_back(std::make_unique<ControllerPaletteItem>(*this, ControllerType::Empty, "Empty"));
    controllerPaletteItems_.push_back(std::make_unique<ControllerPaletteItem>(*this, ControllerType::Rotary, "Rotary"));
    controllerPaletteItems_.push_back(std::make_unique<ControllerPaletteItem>(*this, ControllerType::Button, "Button"));
    controllerPaletteItems_.push_back(std::make_unique<ControllerPaletteItem>(*this, ControllerType::Dropdown, "Dropdown"));
    for (auto const& envelopeSpec : envelopeVariants())
    {
        auto item = std::make_unique<ControllerPaletteItem>(*this,
                                                            ControllerType::Envelope,
                                                            envelopeSpec.paletteLabel,
                                                            envelopeSpec.id);
        item->setTooltipText(envelopeSpec.description);
        controllerPaletteItems_.push_back(std::move(item));
    }
    for (auto& item : controllerPaletteItems_)
        paletteContainer_->addAndMakeVisible(item.get());

    totalSlots_ = gridRows_ * gridCols_;
    slots_.resize(totalSlots_);
    rotaryKnobs_.ensureStorageAllocated(totalSlots_);
    buttonControls_.ensureStorageAllocated(totalSlots_);
    dropdownControls_.ensureStorageAllocated(totalSlots_);

    for (int slotIndex = 0; slotIndex < totalSlots_; ++slotIndex)
    {
        auto rotary = new RotaryWithLabel();
        rotary->setLookAndFeel(&gModernRotaryLookAndFeel);
        rotary->setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.92f));
        rotaryKnobs_.add(rotary);
        addAndMakeVisible(rotary);

        auto button = new ButtonWithLabel();
        button->button_.setClickingTogglesState(true);
        button->button_.setColour(juce::TextButton::textColourOffId, juce::Colours::white.withAlpha(0.92f));
        button->button_.setColour(juce::TextButton::textColourOnId, juce::Colours::white.withAlpha(0.92f));
        button->setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        button->button_.setColour(juce::TextButton::buttonOnColourId, kAccentColour.withAlpha(0.85f));
        //button->button_.setColour(juce::TextButton::buttonColourId, kAccentColour);
        button->button_.setButtonText("Button " + juce::String(slotIndex + 1));
        button->button_.onClick = [this, slotIndex]() { handlePressSlotClick(slotIndex); };
        buttonControls_.add(button);
        addAndMakeVisible(button);

        auto dropdown = new DropdownWithLabel();
        dropdownControls_.add(dropdown);
        addAndMakeVisible(dropdown);
        dropdown->setVisible(false);
        dropdown->setUnused();

        auto envelope = new EnvelopeControl();
        envelopeControls_.add(envelope);
        addAndMakeVisible(envelope);
        envelope->setVisible(false);

        auto dropZone = new juce::Label();
        dropZone->setText("drop zone", juce::dontSendNotification);
        dropZone->setFont(juce::Font(13.0f, juce::Font::italic));
        dropZone->setJustificationType(juce::Justification::centred);
        dropZone->setInterceptsMouseClicks(false, false);
        dropZone->setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.65f));
        dropZone->setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
        //dropZone->setColour(juce::Label::outlineColourId, juce::Colours::white.withAlpha(0.2f));
        //dropZone->setBorderSize(juce::BorderSize<int>(1));
        dropZoneLabels_.add(dropZone);
        addAndMakeVisible(dropZone);

        auto& slot = slots_[slotIndex];
        slot.type = ControllerType::Empty;
        slot.rotary = rotary;
        slot.button = button;
        slot.dropdown = dropdown;
        slot.envelope = envelope;
        slot.buttonDefaultText = button->button_.getButtonText();
        slot.dropZoneLabel = dropZone;
        resetButtonSlotState(slotIndex);
        resetDropdownSlotState(slotIndex);
        resetEnvelopeSlotState(slotIndex);
    }

    initialiseControllerSlots();
    updateAssignmentHighlight();

    LambdaButtonStrip::TButtonMap buttons = {
        { "newLayout", { "New layout", [this]() { handleNewLayoutRequested(); } } },
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
    gridBounds_ = gridArea.toNearestInt();
    cellWidth_ = cellWidth;
    cellHeight_ = cellHeight;

    for (int row = 0; row < gridRows_; ++row)
    {
        for (int col = 0; col < gridCols_; ++col)
        {
            int slotIndex = row * gridCols_ + col;
            if (slotIndex >= totalSlots_)
                continue;

            if (isPlaceholderSlot(slotIndex))
                continue;

            auto& slot = slots_[slotIndex];
            int spanCols = juce::jlimit(1, gridCols_ - col, slot.colSpan);
            int spanRows = juce::jlimit(1, gridRows_ - row, slot.rowSpan);

            juce::Rectangle<float> cell(gridArea.getX() + col * cellWidth,
                                        gridArea.getY() + row * cellHeight,
                                        cellWidth * (float)spanCols,
                                        cellHeight * (float)spanRows);
            auto cellBounds = cell.toNearestInt().reduced(LAYOUT_INSET_SMALL);
            if (slot.rotary != nullptr)
                slot.rotary->setBounds(cellBounds);
            if (slot.button != nullptr)
                slot.button->setBounds(cellBounds.withSizeKeepingCentre(int(cellBounds.getWidth() * 0.8f), LAYOUT_BUTTON_HEIGHT * 2));
            if (slot.dropdown != nullptr)
                slot.dropdown->setBounds(cellBounds.withSizeKeepingCentre(int(cellBounds.getWidth() * 0.85f), LAYOUT_BUTTON_HEIGHT + LAYOUT_LINE_SPACING));
            if (slot.envelope != nullptr)
                slot.envelope->setBounds(cellBounds);
            if (slot.dropZoneLabel != nullptr)
                slot.dropZoneLabel->setBounds(cellBounds);
        }
    }
}

void EditorView::paintOverChildren(juce::Graphics& g)
{
    if (hoverHighlightBounds_.isEmpty())
        return;

    auto bounds = hoverHighlightBounds_.toFloat().reduced(2.0f);
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
    juce::String variantId;
    controllerTypeFromDescription(details.description, isController, variantId);
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
    juce::String variantId;
    auto controllerType = controllerTypeFromDescription(details.description, isController, variantId);
    if (isController)
    {
        handleControllerDrop(slotIndex, controllerType, variantId);
        clearDropHoverState();
        return;
    }

    auto parameter = findParameterByName(details.description.toString());
    if (!parameter)
        return;

    if (slots_[slotIndex].type == ControllerType::Envelope)
    {
        if (lastHitEnvelopeStageIndex_ < 0)
        {
            clearDropHoverState();
            return;
        }
        assignParameterToSlot(slotIndex, parameter, true, lastHitEnvelopeStageIndex_);
    }
    else
    {
        assignParameterToSlot(slotIndex, parameter, true);
    }
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

void EditorView::assignParameterToSlot(int slotIndex, std::shared_ptr<TypedNamedValue> param, bool updateStorage, int envelopeStageIndex)
{
    if (!param || slotIndex < 0 || slotIndex >= totalSlots_)
        return;

    slotIndex = anchorIndexForSlot(slotIndex);
    if (slotIndex < 0 || slotIndex >= totalSlots_)
        return;

    if (slots_[slotIndex].type == ControllerType::Envelope)
    {
        assignParameterToEnvelopeStage(slotIndex, envelopeStageIndex, param, updateStorage);
        return;
    }

    if (slots_[slotIndex].type == ControllerType::Empty)
    {
        setSlotType(slotIndex, ControllerType::Rotary, updateStorage);
    }

    auto& slot = slots_[slotIndex];
    auto newName = param->name().toStdString();

    if (slot.type == ControllerType::Button && !canAssignToPress(*param))
    {
        spdlog::warn("Parameter {} is not suitable for a button controller", newName);
        return;
    }
    if (slot.type == ControllerType::Dropdown && !canAssignToDropdown(*param))
    {
        spdlog::warn("Parameter {} is not suitable for a dropdown controller", newName);
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
    else if (slot.type == ControllerType::Button)
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
        slot.button->button_.setTooltip("Controls " + param->name());
    }
    else if (slot.type == ControllerType::Dropdown)
    {
        slot.dropdownBinding.listener.reset();
        slot.dropdownBinding.param = param;

        auto weakParam = std::weak_ptr<TypedNamedValue>(param);
        slot.dropdown->configureForLookup(param->name(),
                                          param->lookup(),
                                          [weakParam](int newValue) {
                                              if (auto locked = weakParam.lock())
                                              {
                                                  if (locked->value().getValue().operator int() != newValue)
                                                      locked->value().setValue(newValue);
                                              }
                                          });

        slot.dropdownBinding.listener = std::make_unique<LambdaValueListener>(param->value(), [this, slotIndex](juce::Value&) {
            refreshDropdownSlot(slotIndex);
        });

        slot.dropdown->setTooltip("Controls " + param->name());
        refreshDropdownSlot(slotIndex);
    }

    if (updateStorage && !loadingAssignments_)
    {
        storeSlotAssignment(slotIndex);
        markAssignmentsDirty();
    }
    updateAssignmentHighlight();
}

void EditorView::assignParameterToEnvelopeStage(int slotIndex, int stageIndex, std::shared_ptr<TypedNamedValue> param, bool updateStorage)
{
    if (!param || slotIndex < 0 || slotIndex >= totalSlots_)
        return;

    slotIndex = anchorIndexForSlot(slotIndex);
    if (slotIndex < 0 || slotIndex >= totalSlots_)
        return;

    auto& slot = slots_[slotIndex];
    if (slot.type != ControllerType::Envelope || slot.envelope == nullptr)
        return;

    auto variantId = slot.envelopeBinding.variantId;
    if (variantId.isEmpty())
        variantId = defaultEnvelopeVariant().id;

    configureEnvelopeSlot(slotIndex, variantId);

    auto stageCount = static_cast<int>(slot.envelopeBinding.stages.size());
    if (stageIndex < 0 || stageIndex >= stageCount)
        return;

    auto& stageBinding = slot.envelopeBinding.stages[(size_t)stageIndex];
    replaceAssignmentName(stageBinding.assignmentName, param->name().toStdString());

    stageBinding.listener.reset();
    stageBinding.param = param;

    double normalisedValue = normaliseParameterValue(*param);
    slot.envelope->setStageAssignment(stageIndex, param->name(), normalisedValue, true);

    stageBinding.listener = std::make_unique<LambdaValueListener>(param->value(), [this, slotIndex, stageIndex](juce::Value&) {
        refreshEnvelopeStage(slotIndex, stageIndex);
    });

    refreshEnvelopeStage(slotIndex, stageIndex);

    if (updateStorage && !loadingAssignments_)
    {
        storeSlotAssignment(slotIndex);
        markAssignmentsDirty();
    }
    updateAssignmentHighlight();
}

double EditorView::normaliseParameterValue(TypedNamedValue& param) const
{
    auto valueVar = param.value().getValue();
    double value = 0.0;
    if (valueVar.isDouble())
        value = static_cast<double>(valueVar);
    else if (valueVar.isInt() || valueVar.isInt64())
        value = static_cast<double>(static_cast<int>(valueVar));
    else if (valueVar.isBool())
        value = static_cast<bool>(valueVar) ? 1.0 : 0.0;
    else if (valueVar.isString())
    {
        auto stringValue = valueVar.toString().toStdString();
        int index = param.indexOfValue(stringValue);
        if (index >= 0)
            value = static_cast<double>(index);
        else
            value = static_cast<double>(param.minValue());
    }
    else
    {
        value = static_cast<double>(param.minValue());
    }

    double minValue = static_cast<double>(param.minValue());
    double maxValue = static_cast<double>(param.maxValue());
    double range = maxValue - minValue;
    if (std::abs(range) < std::numeric_limits<double>::epsilon())
        return 0.0;

    return juce::jlimit(0.0, 1.0, (value - minValue) / range);
}

bool EditorView::canAssignToPress(const TypedNamedValue& param) const
{
    if (param.valueType() == ValueType::Bool)
        return true;
    int offValue = 0;
    int onValue = 0;
    return extractBinaryValues(param, offValue, onValue);
}

bool EditorView::canAssignToDropdown(const TypedNamedValue& param) const
{
    return param.valueType() == ValueType::Lookup;
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
    slot.button->button_.setToggleState(isOn, juce::dontSendNotification);
    slot.button->button_.setButtonText(buttonValueText(*slot.pressBinding.param, valueVar));
    slot.button->label_.setText(slot.pressBinding.param->name(), dontSendNotification);
}

void EditorView::refreshDropdownSlot(int slotIndex)
{
    if (slotIndex < 0 || slotIndex >= totalSlots_)
        return;

    auto& slot = slots_[slotIndex];
    if (!slot.dropdown || !slot.dropdownBinding.param)
        return;

    auto valueVar = slot.dropdownBinding.param->value().getValue();
    int selectedValue = slot.dropdownBinding.param->minValue();

    if (valueVar.isInt() || valueVar.isInt64())
        selectedValue = static_cast<int>(valueVar);
    else if (valueVar.isDouble())
        selectedValue = static_cast<int>(valueVar);
    else if (valueVar.isString())
        selectedValue = slot.dropdownBinding.param->indexOfValue(valueVar.toString().toStdString());

    slot.dropdown->setSelectedLookupValue(selectedValue);
}

void EditorView::refreshEnvelopeStage(int slotIndex, int stageIndex)
{
    if (slotIndex < 0 || slotIndex >= totalSlots_)
        return;

    auto& slot = slots_[slotIndex];
    if (slot.type != ControllerType::Envelope || slot.envelope == nullptr)
        return;

    auto stageCount = static_cast<int>(slot.envelopeBinding.stages.size());
    if (stageIndex < 0 || stageIndex >= stageCount)
        return;

    auto& stageBinding = slot.envelopeBinding.stages[(size_t)stageIndex];
    if (!stageBinding.param)
        return;

    double normalisedValue = normaliseParameterValue(*stageBinding.param);
    slot.envelope->setStageValue(stageIndex, normalisedValue);
}

void EditorView::handlePressSlotClick(int slotIndex)
{
    if (slotIndex < 0 || slotIndex >= totalSlots_)
        return;

    auto& slot = slots_[slotIndex];
    auto& binding = slot.pressBinding;
    if (!binding.param)
    {
        slot.button->button_.setToggleState(false, juce::dontSendNotification);
        return;
    }

    bool shouldBeOn = slot.button->button_.getToggleState();
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
        resized();
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
    resized();
}

void EditorView::applyAssignmentsToCurrentSynth()
{
    initialiseControllerSlots();

    if (!currentLayoutNode_.isValid())
    {
        updateAssignmentHighlight();
        resized();
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
    resized();
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
    resized();
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

    if (controllerType == ControllerType::Envelope)
    {
        juce::String variantId = assignmentNode.hasProperty(kControllerVariantProperty)
                                     ? assignmentNode.getProperty(kControllerVariantProperty).toString()
                                     : defaultEnvelopeVariant().id;
        setSlotType(slotIndex, ControllerType::Envelope, false, variantId);

        auto& slot = slots_[slotIndex];
        bool assignedAnyStage = false;
        for (size_t stage = 0; stage < slot.envelopeBinding.stages.size(); ++stage)
        {
            auto propertyId = envelopeStagePropertyId(static_cast<int>(stage));
            if (!assignmentNode.hasProperty(propertyId))
                continue;

            auto parameterName = assignmentNode.getProperty(propertyId).toString().toStdString();
            if (parameterName.empty() || !uiModel_.hasValue(parameterName))
                continue;

            assignParameterToSlot(slotIndex, uiModel_.typedNamedValueByName(parameterName), false, static_cast<int>(stage));
            assignedAnyStage = true;
        }

        if (!assignedAnyStage && slot.envelopeBinding.stages.size() == kLegacyEnvelopeStageProperties.size())
        {
            for (size_t stage = 0; stage < kLegacyEnvelopeStageProperties.size(); ++stage)
            {
                auto legacyId = kLegacyEnvelopeStageProperties[stage];
                if (!assignmentNode.hasProperty(legacyId))
                    continue;

                auto parameterName = assignmentNode.getProperty(legacyId).toString().toStdString();
                if (parameterName.empty() || !uiModel_.hasValue(parameterName))
                    continue;

                assignParameterToSlot(slotIndex, uiModel_.typedNamedValueByName(parameterName), false, static_cast<int>(stage));
            }
        }
    }
    else if (assignmentNode.hasProperty(kParameterProperty))
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

    auto controllerType = slots_[slotIndex].type;
    assignment.setProperty(kControllerProperty, controllerTypeToString(controllerType), nullptr);

    if (controllerType == ControllerType::Envelope)
    {
        assignment.setProperty(kControllerVariantProperty, slots_[slotIndex].envelopeBinding.variantId, nullptr);
        assignment.removeProperty(kParameterProperty, nullptr);
        for (int i = assignment.getNumProperties() - 1; i >= 0; --i)
        {
            auto propertyName = assignment.getPropertyName(i);
            if (propertyName.toString().startsWith("envelopeStage"))
                assignment.removeProperty(propertyName, nullptr);
        }
        for (auto const& legacyId : kLegacyEnvelopeStageProperties)
            assignment.removeProperty(legacyId, nullptr);

        for (size_t stage = 0; stage < slots_[slotIndex].envelopeBinding.stages.size(); ++stage)
        {
            auto const& name = slots_[slotIndex].envelopeBinding.stages[stage].assignmentName;
            if (!name.empty())
            {
                auto propertyId = envelopeStagePropertyId(static_cast<int>(stage));
                assignment.setProperty(propertyId, juce::String(name), nullptr);
            }
        }
    }
    else
    {
        assignment.removeProperty(kControllerVariantProperty, nullptr);
        if (slots_[slotIndex].assignedParameter.empty())
            assignment.removeProperty(kParameterProperty, nullptr);
        else
            assignment.setProperty(kParameterProperty, juce::String(slots_[slotIndex].assignedParameter), nullptr);

        for (int i = assignment.getNumProperties() - 1; i >= 0; --i)
        {
            auto propertyName = assignment.getPropertyName(i);
            if (propertyName.toString().startsWith("envelopeStage"))
                assignment.removeProperty(propertyName, nullptr);
        }
        for (auto const& legacyId : kLegacyEnvelopeStageProperties)
            assignment.removeProperty(legacyId, nullptr);
    }
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

void EditorView::handleNewLayoutRequested()
{
    if (!assignmentsLoaded_)
        loadAssignmentsFromDisk();

    auto shouldClear = juce::AlertWindow::showOkCancelBox(juce::AlertWindow::QuestionIcon,
                                                          "Clear layout",
                                                          "Do you really want to remove all controllers from the grid?");
    if (!shouldClear)
        return;

    clearAllSlots();
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

void EditorView::configureEnvelopeSlot(int slotIndex, const juce::String& variantId)
{
    if (slotIndex < 0 || slotIndex >= totalSlots_)
        return;

    auto& slot = slots_[slotIndex];
    if (slot.envelope == nullptr)
        return;

    const auto& variant = envelopeVariantById(variantId.isEmpty() ? slot.envelopeBinding.variantId : variantId);
    bool variantChanged = !slot.envelopeBinding.variantId.equalsIgnoreCase(variant.id);
    bool stageCountChanged = slot.envelope->stageCount() != static_cast<int>(variant.stages.size());
    slot.envelopeBinding.variantId = variant.id;
    if (variantChanged || stageCountChanged)
        slot.envelope->setSpecification(makeSpecificationFromVariant(variant));

    auto targetSize = variant.stages.size();
    auto currentSize = slot.envelopeBinding.stages.size();

    if (variantChanged)
    {
        for (auto& binding : slot.envelopeBinding.stages)
        {
            if (!binding.assignmentName.empty())
                replaceAssignmentName(binding.assignmentName, "");
        }
        slot.envelopeBinding.stages.clear();
        slot.envelopeBinding.stages.resize(targetSize);
    }
    else if (currentSize > targetSize)
    {
        for (size_t i = targetSize; i < currentSize; ++i)
        {
            auto& binding = slot.envelopeBinding.stages[i];
            if (!binding.assignmentName.empty())
                replaceAssignmentName(binding.assignmentName, "");
        }
        slot.envelopeBinding.stages.resize(targetSize);
    }
    else if (currentSize < targetSize)
    {
        auto previousSize = currentSize;
        slot.envelopeBinding.stages.resize(targetSize);
        for (size_t i = previousSize; i < targetSize; ++i)
            slot.envelopeBinding.stages[i] = {};
    }
}

void EditorView::resetEnvelopeSlotState(int slotIndex)
{
    if (slotIndex < 0 || slotIndex >= totalSlots_)
        return;

    auto& slot = slots_[slotIndex];
    if (slot.envelope == nullptr)
        return;

    auto variantId = slot.envelopeBinding.variantId;
    if (variantId.isEmpty())
        variantId = defaultEnvelopeVariant().id;

    configureEnvelopeSlot(slotIndex, variantId);

    for (size_t stage = 0; stage < slot.envelopeBinding.stages.size(); ++stage)
    {
        auto& stageBinding = slot.envelopeBinding.stages[stage];
        if (!stageBinding.assignmentName.empty())
            replaceAssignmentName(stageBinding.assignmentName, "");
        stageBinding.listener.reset();
        stageBinding.param.reset();
        stageBinding.assignmentName.clear();
        slot.envelope->clearStage((int)stage);
    }
    slot.envelope->setHoveredStage(-1);
}

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
        slot.button->button_.setToggleState(false, juce::dontSendNotification);
        slot.button->button_.setButtonText(defaultButtonStateText(slot, false));
        slot.button->label_.setText("", dontSendNotification);
        slot.button->button_.setTooltip({});
    }
}

void EditorView::resetDropdownSlotState(int slotIndex)
{
    if (slotIndex < 0 || slotIndex >= totalSlots_)
        return;

    auto& slot = slots_[slotIndex];
    if (slot.dropdownBinding.listener)
        slot.dropdownBinding.listener.reset();
    slot.dropdownBinding.param.reset();

    if (slot.dropdown != nullptr)
    {
        slot.dropdown->setUnused();
        slot.dropdown->setTooltip({});
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

juce::String EditorView::defaultButtonStateText(const ControllerSlot& slot, bool isOn) const
{
    juce::String stateText = isOn ? "On" : "Off";
    if (slot.buttonDefaultText.isEmpty())
        return stateText;
    return slot.buttonDefaultText + " (" + stateText + ")";
}

void EditorView::initialiseControllerSlots()
{
    assignmentUsage_.clear();
    for (int i = 0; i < totalSlots_; ++i)
    {
        auto& slot = slots_[i];
        replaceAssignmentName(slot.assignedParameter, "");
        slot.type = ControllerType::Empty;
        resetButtonSlotState(i);
        resetDropdownSlotState(i);
        resetEnvelopeSlotState(i);
        if (slot.rotary != nullptr)
            slot.rotary->setUnused();
        slot.rowSpan = 1;
        slot.colSpan = 1;
        slot.anchorIndex = i;
        slot.placeholder = false;
        updateSlotVisibility(i);
    }
}

void EditorView::clearAllSlots()
{
    clearDropHoverState();
    initialiseControllerSlots();
    resized();

    if (currentLayoutNode_.isValid())
    {
        if (auto slotsNode = currentLayoutNode_.getChildWithName(kSlotsNodeId); slotsNode.isValid())
            currentLayoutNode_.removeChild(slotsNode, nullptr);
    }

    markAssignmentsDirty();
    updateAssignmentHighlight();
    repaint();
}

void EditorView::updateSlotVisibility(int slotIndex)
{
    if (slotIndex < 0 || slotIndex >= totalSlots_)
        return;

    auto& slot = slots_[slotIndex];
    if (slot.placeholder)
    {
        if (slot.rotary != nullptr)
        {
            slot.rotary->setVisible(false);
            slot.rotary->setUnused();
        }
        if (slot.button != nullptr)
            slot.button->setVisible(false);
        if (slot.dropdown != nullptr)
        {
            slot.dropdown->setVisible(false);
            slot.dropdown->setUnused();
        }
        if (slot.envelope != nullptr)
            slot.envelope->setVisible(false);
        if (slot.dropZoneLabel != nullptr)
            slot.dropZoneLabel->setVisible(false);
        return;
    }
    if (slot.rotary != nullptr)
    {
        slot.rotary->setVisible(slot.type == ControllerType::Rotary);
        if (slot.type != ControllerType::Rotary)
            slot.rotary->setUnused();
    }
    if (slot.button != nullptr)
        slot.button->setVisible(slot.type == ControllerType::Button);
    if (slot.dropdown != nullptr)
    {
        slot.dropdown->setVisible(slot.type == ControllerType::Dropdown);
        if (slot.type != ControllerType::Dropdown)
            slot.dropdown->setUnused();
    }
    if (slot.envelope != nullptr)
        slot.envelope->setVisible(slot.type == ControllerType::Envelope);
    if (slot.dropZoneLabel != nullptr)
        slot.dropZoneLabel->setVisible(slot.type == ControllerType::Empty);
}

void EditorView::setSlotType(int slotIndex, ControllerType type, bool recordChange, const juce::String& variantId)
{
    slotIndex = anchorIndexForSlot(slotIndex);
    if (slotIndex < 0 || slotIndex >= totalSlots_)
        return;

    auto& slot = slots_[slotIndex];
    juce::String desiredVariant = variantId;
    if (type == ControllerType::Envelope && desiredVariant.isEmpty())
        desiredVariant = slot.envelopeBinding.variantId.isEmpty() ? defaultEnvelopeVariant().id : slot.envelopeBinding.variantId;

    bool typeUnchanged = (slot.type == type);
    bool variantUnchanged = (type != ControllerType::Envelope) || (slot.envelopeBinding.variantId.equalsIgnoreCase(desiredVariant));

    if (typeUnchanged && variantUnchanged)
        return;

    bool hadSpan = slot.rowSpan > 1 || slot.colSpan > 1;
    if (hadSpan)
        releaseSpanForAnchor(slotIndex);

    if (slot.type == ControllerType::Envelope)
        resetEnvelopeSlotState(slotIndex);
    else if (!slot.assignedParameter.empty())
        replaceAssignmentName(slot.assignedParameter, "");

    if (slot.type == ControllerType::Button)
        resetButtonSlotState(slotIndex);
    if (slot.type == ControllerType::Dropdown)
        resetDropdownSlotState(slotIndex);
    if (slot.type == ControllerType::Rotary && slot.rotary != nullptr)
        slot.rotary->setUnused();

    slot.type = type;
    slot.placeholder = false;
    slot.anchorIndex = slotIndex;
    slot.rowSpan = 1;
    slot.colSpan = 1;
    if (slot.type == ControllerType::Button)
        resetButtonSlotState(slotIndex);
    if (slot.type == ControllerType::Dropdown)
        resetDropdownSlotState(slotIndex);
    if (slot.type == ControllerType::Rotary && slot.rotary != nullptr)
        slot.rotary->setUnused();
    if (slot.type == ControllerType::Envelope)
    {
        configureEnvelopeSlot(slotIndex, desiredVariant);
        resetEnvelopeSlotState(slotIndex);
        slot.assignedParameter.clear();
        applySpanForAnchor(slotIndex, kEnvelopeRowSpan, kEnvelopeColSpan);
    }
    updateSlotVisibility(slotIndex);

    if (recordChange && !loadingAssignments_)
    {
        storeSlotAssignment(slotIndex);
        markAssignmentsDirty();
        updateAssignmentHighlight();
    }
}

juce::Component* EditorView::primaryComponentForSlot(int slotIndex) const
{
    if (slotIndex < 0 || slotIndex >= totalSlots_)
        return nullptr;

    int anchorIndex = anchorIndexForSlot(slotIndex);
    if (anchorIndex < 0 || anchorIndex >= totalSlots_)
        return nullptr;

    auto const& slot = slots_[anchorIndex];
    switch (slot.type)
    {
    case ControllerType::Empty:
        return slot.dropZoneLabel;
    case ControllerType::Rotary:
        return slot.rotary;
    case ControllerType::Button:
        return slot.button;
    case ControllerType::Dropdown:
        return slot.dropdown;
    case ControllerType::Envelope:
        return slot.envelope;
    default:
        break;
    }
    return nullptr;
}

juce::Rectangle<int> EditorView::boundsForSpan(int anchorIndex, int rowSpan, int colSpan) const
{
    if (anchorIndex < 0 || anchorIndex >= totalSlots_)
        return {};
    if (cellWidth_ <= 0.0f || cellHeight_ <= 0.0f)
        return {};

    int anchorRow = anchorIndex / gridCols_;
    int anchorCol = anchorIndex % gridCols_;

    auto bounds = juce::Rectangle<float>(static_cast<float>(gridBounds_.getX()) + anchorCol * cellWidth_,
                                         static_cast<float>(gridBounds_.getY()) + anchorRow * cellHeight_,
                                         cellWidth_ * static_cast<float>(colSpan),
                                         cellHeight_ * static_cast<float>(rowSpan));
    return bounds.toNearestInt().reduced(LAYOUT_INSET_SMALL);
}

int EditorView::slotIndexForComponent(juce::Component* component) const
{
    for (int i = 0; i < totalSlots_; ++i)
    {
        if (slots_[i].rotary == component || slots_[i].button == component || slots_[i].dropdown == component || slots_[i].envelope == component || slots_[i].dropZoneLabel == component)
        {
            if (isPlaceholderSlot(i))
                return anchorIndexForSlot(i);
            return i;
        }
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

bool EditorView::isPlaceholderSlot(int slotIndex) const
{
    if (slotIndex < 0 || slotIndex >= totalSlots_)
        return false;
    return slots_[slotIndex].placeholder;
}

int EditorView::anchorIndexForSlot(int slotIndex) const
{
    if (slotIndex < 0 || slotIndex >= totalSlots_)
        return -1;

    auto const& slot = slots_[slotIndex];
    if (slot.placeholder)
    {
        if (slot.anchorIndex >= 0 && slot.anchorIndex < totalSlots_)
            return slot.anchorIndex;
        return -1;
    }
    return slotIndex;
}

int EditorView::clampAnchorIndexForSpan(int slotIndex, int rowSpan, int colSpan) const
{
    if (slotIndex < 0 || slotIndex >= totalSlots_)
        return slotIndex;

    rowSpan = juce::jlimit(1, gridRows_, rowSpan);
    colSpan = juce::jlimit(1, gridCols_, colSpan);

    int row = slotIndex / gridCols_;
    int col = slotIndex % gridCols_;

    int maxRowAnchor = juce::jmax(0, gridRows_ - rowSpan);
    int maxColAnchor = juce::jmax(0, gridCols_ - colSpan);

    int anchorRow = juce::jlimit(0, maxRowAnchor, row);
    int anchorCol = juce::jlimit(0, maxColAnchor, col);

    return anchorRow * gridCols_ + anchorCol;
}

void EditorView::clearAnchorsWithinSpan(int anchorIndex, int rowSpan, int colSpan)
{
    if (anchorIndex < 0 || anchorIndex >= totalSlots_)
        return;

    rowSpan = juce::jlimit(1, gridRows_, rowSpan);
    colSpan = juce::jlimit(1, gridCols_, colSpan);

    int anchorRow = anchorIndex / gridCols_;
    int anchorCol = anchorIndex % gridCols_;

    rowSpan = juce::jlimit(1, gridRows_ - anchorRow, rowSpan);
    colSpan = juce::jlimit(1, gridCols_ - anchorCol, colSpan);

    std::vector<int> anchorsToClear;
    for (int r = 0; r < rowSpan; ++r)
    {
        for (int c = 0; c < colSpan; ++c)
        {
            int idx = (anchorRow + r) * gridCols_ + (anchorCol + c);
            if (idx < 0 || idx >= totalSlots_)
                continue;

            int otherAnchor = anchorIndexForSlot(idx);
            if (otherAnchor == anchorIndex || otherAnchor < 0)
                continue;

            if (std::find(anchorsToClear.begin(), anchorsToClear.end(), otherAnchor) == anchorsToClear.end())
                anchorsToClear.push_back(otherAnchor);
        }
    }

    for (int otherAnchor : anchorsToClear)
        setSlotType(otherAnchor, ControllerType::Empty, true);
}

void EditorView::releaseSpanForAnchor(int anchorIndex)
{
    if (anchorIndex < 0 || anchorIndex >= totalSlots_)
        return;

    auto& anchor = slots_[anchorIndex];
    int rowSpan = juce::jmax(1, anchor.rowSpan);
    int colSpan = juce::jmax(1, anchor.colSpan);
    int anchorRow = anchorIndex / gridCols_;
    int anchorCol = anchorIndex % gridCols_;

    for (int r = 0; r < rowSpan; ++r)
    {
        for (int c = 0; c < colSpan; ++c)
        {
            int idx = (anchorRow + r) * gridCols_ + (anchorCol + c);
            if (idx < 0 || idx >= totalSlots_ || idx == anchorIndex)
                continue;

            auto& slot = slots_[idx];
            if (slot.anchorIndex == anchorIndex && slot.placeholder)
            {
                slot.placeholder = false;
                slot.anchorIndex = idx;
                slot.rowSpan = 1;
                slot.colSpan = 1;
                updateSlotVisibility(idx);
            }
        }
    }

    anchor.rowSpan = 1;
    anchor.colSpan = 1;
    anchor.anchorIndex = anchorIndex;
    anchor.placeholder = false;
    updateSlotVisibility(anchorIndex);
}

void EditorView::applySpanForAnchor(int anchorIndex, int rowSpan, int colSpan)
{
    if (anchorIndex < 0 || anchorIndex >= totalSlots_)
        return;

    auto& anchor = slots_[anchorIndex];
    int anchorRow = anchorIndex / gridCols_;
    int anchorCol = anchorIndex % gridCols_;

    rowSpan = juce::jlimit(1, gridRows_, rowSpan);
    colSpan = juce::jlimit(1, gridCols_, colSpan);
    rowSpan = juce::jlimit(1, gridRows_ - anchorRow, rowSpan);
    colSpan = juce::jlimit(1, gridCols_ - anchorCol, colSpan);

    anchor.rowSpan = rowSpan;
    anchor.colSpan = colSpan;
    anchor.anchorIndex = anchorIndex;
    anchor.placeholder = false;

    for (int r = 0; r < rowSpan; ++r)
    {
        for (int c = 0; c < colSpan; ++c)
        {
            int idx = (anchorRow + r) * gridCols_ + (anchorCol + c);
            if (idx < 0 || idx >= totalSlots_ || idx == anchorIndex)
                continue;

            auto& slot = slots_[idx];
            slot.placeholder = true;
            slot.anchorIndex = anchorIndex;
            slot.rowSpan = 1;
            slot.colSpan = 1;
            if (!slot.assignedParameter.empty())
                replaceAssignmentName(slot.assignedParameter, "");
            slot.type = ControllerType::Empty;
            if (slot.dropZoneLabel != nullptr)
                slot.dropZoneLabel->setBounds({});
            updateSlotVisibility(idx);
        }
    }

    updateSlotVisibility(anchorIndex);
}

int EditorView::slotIndexAt(juce::Point<int> localPos) const
{
    lastHitEnvelopeStageIndex_ = -1;
    for (int i = 0; i < totalSlots_; ++i)
    {
        if (isPlaceholderSlot(i))
            continue;
        auto const& slot = slots_[i];
        juce::Component* component = nullptr;
        switch (slot.type)
        {
        case ControllerType::Empty:
            component = slot.dropZoneLabel;
            break;
        case ControllerType::Rotary:
            component = slot.rotary;
            break;
        case ControllerType::Button:
            component = slot.button;
            break;
        case ControllerType::Dropdown:
            component = slot.dropdown;
            break;
        case ControllerType::Envelope:
            component = slot.envelope;
            break;
        }
        if (component != nullptr && component->isShowing() && component->getBounds().contains(localPos))
        {
            if (slot.type == ControllerType::Envelope && slot.envelope != nullptr)
            {
                auto localPoint = slot.envelope->getLocalPoint(this, localPos).toFloat();
                lastHitEnvelopeStageIndex_ = slot.envelope->stageAtLocalPoint(localPoint);
            }
            return i;
        }
    }
    return -1;
}

void EditorView::handleControllerDrop(int slotIndex, ControllerType type, const juce::String& variantId)
{
    if (slotIndex < 0 || slotIndex >= totalSlots_)
        return;

    slotIndex = anchorIndexForSlot(slotIndex);
    if (slotIndex < 0 || slotIndex >= totalSlots_)
        return;

    if (type == ControllerType::Envelope)
    {
        slotIndex = clampAnchorIndexForSpan(slotIndex, kEnvelopeRowSpan, kEnvelopeColSpan);
        clearAnchorsWithinSpan(slotIndex, kEnvelopeRowSpan, kEnvelopeColSpan);
    }

    auto previousType = slots_[slotIndex].type;
    std::shared_ptr<TypedNamedValue> preservedParam;
    if (previousType != ControllerType::Envelope)
    {
        auto existingName = slots_[slotIndex].assignedParameter;
        if (!existingName.empty())
            preservedParam = findParameterByName(existingName);
    }

    bool preserveAssignment = preservedParam && shouldPreserveAssignment(previousType, type, *preservedParam);
    bool typeChanged = previousType != type;

    setSlotType(slotIndex, type, true, variantId);

    if (preserveAssignment && preservedParam && type != ControllerType::Envelope)
    {
        assignParameterToSlot(slotIndex, preservedParam, true);
    }
    else
    {
        if (slots_[slotIndex].type == ControllerType::Envelope)
        {
            resetEnvelopeSlotState(slotIndex);
        }
        else
        {
            replaceAssignmentName(slots_[slotIndex].assignedParameter, "");
            if (slots_[slotIndex].type == ControllerType::Button)
                resetButtonSlotState(slotIndex);
            else if (slots_[slotIndex].type == ControllerType::Dropdown)
                resetDropdownSlotState(slotIndex);
            else if (slots_[slotIndex].type == ControllerType::Rotary && slots_[slotIndex].rotary != nullptr)
                slots_[slotIndex].rotary->setUnused();
        }

        if (!loadingAssignments_ && !typeChanged)
            storeSlotAssignment(slotIndex);
    }

    markAssignmentsDirty();
    updateAssignmentHighlight();

    if (previousType == ControllerType::Envelope || type == ControllerType::Envelope)
        resized();
}

EditorView::ControllerType EditorView::controllerTypeFromDescription(const juce::var& description,
                                                                     bool& isController,
                                                                     juce::String& variantId) const
{
    isController = false;
    variantId.clear();
    if (!description.isString())
        return ControllerType::Rotary;
    auto text = description.toString();
    juce::StringArray tokens;
    tokens.addTokens(text, ":", "");
    tokens.trim();
    if (tokens.size() < 2 || tokens[0].compareIgnoreCase("controller") != 0)
        return ControllerType::Rotary;

    isController = true;
    auto typeToken = tokens[1];
    if (typeToken.compareIgnoreCase("empty") == 0)
        return ControllerType::Empty;
    if (typeToken.compareIgnoreCase("rotary") == 0)
        return ControllerType::Rotary;
    if (typeToken.compareIgnoreCase("button") == 0)
        return ControllerType::Button;
    if (typeToken.compareIgnoreCase("dropdown") == 0)
        return ControllerType::Dropdown;
    if (typeToken.compareIgnoreCase("envelope") == 0)
    {
        if (tokens.size() >= 3)
            variantId = tokens[2];
        return ControllerType::Envelope;
    }
    if (typeToken.compareIgnoreCase("adsr") == 0)
    {
        variantId = defaultEnvelopeVariant().id;
        return ControllerType::Envelope;
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
    juce::String variantId;
    auto describedType = controllerTypeFromDescription(details.description, isController, variantId);

    auto localPos = mousePositionInLocalSpace();
    auto slotIndex = slotIndexAt(localPos);
    int stageIndex = (slotIndex >= 0 && slotIndex < totalSlots_ && slots_[slotIndex].type == ControllerType::Envelope) ? lastHitEnvelopeStageIndex_ : -1;

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
                else if (targetType == ControllerType::Dropdown)
                    canDrop = canAssignToDropdown(*parameter);
                else if (targetType == ControllerType::Envelope)
                    canDrop = stageIndex >= 0;
                else
                    canDrop = true;
            }
        }
    }

    if (!canDrop)
    {
        slotIndex = -1;
        stageIndex = -1;
    }

    auto highlightBounds = juce::Rectangle<int>();
    if (slotIndex >= 0)
    {
        if (isController && describedType == ControllerType::Envelope)
        {
            int anchorIndex = anchorIndexForSlot(slotIndex);
            if (anchorIndex < 0)
                anchorIndex = slotIndex;
            anchorIndex = clampAnchorIndexForSpan(anchorIndex, kEnvelopeRowSpan, kEnvelopeColSpan);
            highlightBounds = boundsForSpan(anchorIndex, kEnvelopeRowSpan, kEnvelopeColSpan);
        }
        else if (auto* component = primaryComponentForSlot(slotIndex))
        {
            highlightBounds = component->getBounds();
        }
    }

    bool highlightChanged = highlightBounds != hoverHighlightBounds_;
    hoverHighlightBounds_ = highlightBounds;

    if (slotIndex != hoveredSlotIndex_ || stageIndex != hoveredEnvelopeStageIndex_)
    {
        if (hoveredSlotIndex_ >= 0 && hoveredSlotIndex_ < totalSlots_)
        {
            auto& previousSlot = slots_[hoveredSlotIndex_];
            if (previousSlot.type == ControllerType::Envelope && previousSlot.envelope != nullptr)
                previousSlot.envelope->setHoveredStage(-1);
        }

        hoveredSlotIndex_ = slotIndex;
        hoveredEnvelopeStageIndex_ = stageIndex;

        if (hoveredSlotIndex_ >= 0 && hoveredSlotIndex_ < totalSlots_)
        {
            auto& hoveredSlot = slots_[hoveredSlotIndex_];
            if (hoveredSlot.type == ControllerType::Envelope && hoveredSlot.envelope != nullptr)
                hoveredSlot.envelope->setHoveredStage(stageIndex);
        }

        repaint();
    }
    else if (highlightChanged)
    {
        repaint();
    }

    setMouseCursor(canDrop ? juce::MouseCursor::CopyingCursor : juce::MouseCursor::NormalCursor);
}

void EditorView::clearDropHoverState()
{
    int previousSlotIndex = hoveredSlotIndex_;
    bool hadHover = previousSlotIndex != -1 || hoveredEnvelopeStageIndex_ != -1;
    bool hadHighlight = !hoverHighlightBounds_.isEmpty();
    if (previousSlotIndex >= 0 && previousSlotIndex < totalSlots_)
    {
        auto& previousSlot = slots_[previousSlotIndex];
        if (previousSlot.type == ControllerType::Envelope && previousSlot.envelope != nullptr)
            previousSlot.envelope->setHoveredStage(-1);
    }
    hoveredSlotIndex_ = -1;
    hoveredEnvelopeStageIndex_ = -1;
    hoverHighlightBounds_ = {};
    setMouseCursor(juce::MouseCursor::NormalCursor);
    if (hadHover || hadHighlight)
        repaint();
}

bool EditorView::shouldPreserveAssignment(ControllerType fromType, ControllerType toType, const TypedNamedValue& param) const
{
    if (fromType == toType)
        return false;

    if (fromType == ControllerType::Envelope || toType == ControllerType::Envelope)
        return false;

    if (fromType == ControllerType::Rotary && toType == ControllerType::Dropdown)
        return canAssignToDropdown(param);

    if (fromType == ControllerType::Dropdown && toType == ControllerType::Rotary)
        return true;

    return false;
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
        auto& slot = slots_[slotIndex];
        slot.buttonDefaultText = juce::String(name);
        if (!slot.pressBinding.param)
        {
            slot.button->button_.setToggleState(false, juce::dontSendNotification);
            slot.button->button_.setButtonText(defaultButtonStateText(slot, false));
            slot.button->label_.setText("", dontSendNotification);
            slot.button->button_.setTooltip({});
        }
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
