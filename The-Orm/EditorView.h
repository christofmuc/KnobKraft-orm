/*
   Copyright (c) 2020-2025 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "JuceHeader.h"

#include "BCR2000Proxy.h"

#include "BCR2000.h"
#include "LambdaButtonStrip.h"
#include "LambdaValueListener.h"
#include "Librarian.h"
#include "PatchTextBox.h"
#include "ValueTreeViewer.h"

#include <optional>
#include <unordered_map>

class RotaryWithLabel;
class ButtonWithLabel;
class SynthParameterDefinition;
class Synth;

class EditorView : public juce::DragAndDropContainer,
                   public juce::DragAndDropTarget,
                   public midikraft::BCR2000Proxy,
                   public juce::Component,
                   private juce::ChangeListener {
public:
    EditorView(std::shared_ptr<midikraft::BCR2000> bcr);
    ~EditorView() override;

    void resized() override;
    void paintOverChildren(juce::Graphics& g) override;

    void setRotaryParam(int knobNumber, TypedNamedValue* param) override;
    void setButtonParam(int knobNumber, std::string const& name) override;

    void changeListenerCallback(juce::ChangeBroadcaster* source) override;

    bool isInterestedInDragSource(const juce::DragAndDropTarget::SourceDetails& details) override;
    void itemDragEnter(const juce::DragAndDropTarget::SourceDetails& details) override;
    void itemDragMove(const juce::DragAndDropTarget::SourceDetails& details) override;
    void itemDragExit(const juce::DragAndDropTarget::SourceDetails& details) override;
    void itemDropped(const juce::DragAndDropTarget::SourceDetails& details) override;
    void dragOperationEnded(const juce::DragAndDropTarget::SourceDetails& details) override;

    enum class ControllerType {
        Empty,
        Rotary,
        Button,
    };

private:
    struct PressBinding {
        std::shared_ptr<TypedNamedValue> param;
        bool usesBool = false;
        int offValue = 0;
        int onValue = 1;
        std::unique_ptr<LambdaValueListener> listener;
    };

    struct ControllerSlot {
        ControllerType type = ControllerType::Empty;
        RotaryWithLabel* rotary = nullptr;
        ButtonWithLabel* button = nullptr;
        PressBinding pressBinding;
        std::string assignedParameter;
        juce::String buttonDefaultText;
        juce::Label* dropZoneLabel = nullptr;
    };

	class ControllerPaletteItem : public juce::Component {
	public:
		ControllerPaletteItem(EditorView& owner, ControllerType type, juce::String const& labelText);
		void paint(juce::Graphics& g) override;
		void mouseDown(const juce::MouseEvent& event) override;
		void mouseEnter(const juce::MouseEvent&) override;
		void mouseExit(const juce::MouseEvent&) override;

	private:
		EditorView& owner_;
		ControllerType type_;
		juce::String label_;
    };

    class UpdateSynthListener : public juce::ValueTree::Listener {
    public:
        explicit UpdateSynthListener(EditorView* parent);
        ~UpdateSynthListener() override;

        void valueTreePropertyChanged(juce::ValueTree& treeWhosePropertyHasChanged, const juce::Identifier& property) override;
        void listenForMidiMessages(juce::MidiInput* source, juce::MidiMessage message);
        void updateAllKnobsFromPatch(std::shared_ptr<midikraft::Synth> synth, std::shared_ptr<midikraft::DataFile> newPatch);

    private:
        midikraft::MidiController::HandlerHandle midiHandler_ = midikraft::MidiController::makeOneHandle();
        std::shared_ptr<midikraft::DataFile> editBuffer_;
        EditorView* owner_;
    };

    // Helpers
    TypedNamedValueSet createParameterModel();
    std::shared_ptr<TypedNamedValue> findParameterByName(const juce::String& propertyName);
    void assignParameterToSlot(int slotIndex, std::shared_ptr<TypedNamedValue> param, bool updateStorage);
    bool canAssignToPress(const TypedNamedValue& param) const;
    bool extractBinaryValues(const TypedNamedValue& param, int& offValue, int& onValue) const;
    void refreshPressButtonSlot(int slotIndex);
    void handlePressSlotClick(int slotIndex);
    juce::String buttonValueText(const TypedNamedValue& param, const juce::var& value) const;
    void setEditorPatch(std::shared_ptr<midikraft::Synth> synth, std::shared_ptr<midikraft::DataFile> data);
    void refreshEditorPatch();

    void loadAssignmentsForSynth(std::shared_ptr<midikraft::Synth> synth);
    void applyAssignmentsToCurrentSynth();
    void applyAssignmentsFromTree(const juce::ValueTree& layoutTree);
    void applyAssignmentToSlotFromTree(const juce::ValueTree& assignmentNode);
    void storeSlotAssignment(int slotIndex);
    juce::ValueTree ensureLayoutNode(const juce::String& synthName);
    juce::ValueTree findLayoutNode(const juce::String& synthName) const;
    juce::ValueTree ensureSection(juce::ValueTree parent, const juce::Identifier& sectionId);
    juce::ValueTree findAssignmentNode(juce::ValueTree parent, int index) const;
    juce::ValueTree ensureAssignmentNode(juce::ValueTree parent, int index);
    void loadAssignmentsFromDisk();
    void saveAssignmentsToDisk();
    void handleLoadAssignmentsRequested();
    void handleSaveAssignmentsRequested();
    void handleNewLayoutRequested();
    juce::File assignmentsFile() const;
	void markAssignmentsDirty();
	void flushAssignmentsIfDirty();
	void resetButtonSlotState(int slotIndex);
	void updateAssignmentHighlight();
    void incrementAssignment(const std::string& name);
    void decrementAssignment(const std::string& name);
    void replaceAssignmentName(std::string& slotName, const std::string& newName);
    juce::String defaultButtonStateText(const ControllerSlot& slot, bool isOn) const;
    void initialiseControllerSlots();
    void clearAllSlots();
    void updateSlotVisibility(int slotIndex);
    void setSlotType(int slotIndex, ControllerType type, bool recordChange);
    int slotIndexForComponent(juce::Component* component) const;
    int slotIndexFromRotaryIndex(int rotaryIndex) const;
    int slotIndexFromButtonIndex(int buttonIndex) const;
    int slotIndexAt(juce::Point<int> localPos) const;
    void handleControllerDrop(int slotIndex, ControllerType type);
    ControllerType controllerTypeFromDescription(const juce::var& description, bool& isController) const;
    juce::Point<int> mousePositionInLocalSpace() const;
    void updateDropHoverState(const juce::DragAndDropTarget::SourceDetails& details);
    void clearDropHoverState();

    TypedNamedValueSet synthModel_;
    TypedNamedValueSet uiModel_;
    juce::ValueTree uiValueTree_;
    TypedNamedValueSet controllerModel_;

    PatchTextBox patchTextBox_;
    std::shared_ptr<midikraft::PatchHolder> editorPatchHolder_;

    UpdateSynthListener updateSynthListener_;

    ValueTreeViewer valueTreeViewer_;
    juce::OwnedArray<RotaryWithLabel> rotaryKnobs_;
    juce::OwnedArray<ButtonWithLabel> buttonControls_;
    juce::OwnedArray<juce::Label> dropZoneLabels_;
    std::vector<ControllerSlot> slots_;
    std::unique_ptr<juce::Component> paletteContainer_;
    std::vector<std::unique_ptr<ControllerPaletteItem>> controllerPaletteItems_;
    std::unique_ptr<LambdaButtonStrip> buttons_;
    std::shared_ptr<midikraft::BCR2000> bcr2000_;

    midikraft::Librarian librarian_;

    int hoveredSlotIndex_ = -1;

    juce::ValueTree assignmentsRoot_;
    juce::ValueTree currentLayoutNode_;
    juce::String currentSynthName_;
    juce::String currentLayoutId_{ "default" };
    bool assignmentsLoaded_ = false;
    bool assignmentsDirty_ = false;
    bool loadingAssignments_ = false;
    std::unordered_map<std::string, int> assignmentUsage_;

    int gridRows_ = 8;
    int gridCols_ = 8;
    int totalSlots_ = gridRows_ * gridCols_;
};
