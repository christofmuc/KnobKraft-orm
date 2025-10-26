/*
   Copyright (c) 2020-2025 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "JuceHeader.h"

#include "BCR2000Proxy.h"

#include "BCR2000.h"
#include "LambdaButtonStrip.h"
#include "Librarian.h"
#include "ValueTreeViewer.h"

class RotaryWithLabel;
class SynthParameterDefinition;
class Synth;

class EditorView: public juce::DragAndDropContainer,
                  public juce::DragAndDropTarget,
                  public midikraft::BCR2000Proxy,
				  public juce::Component,
                  private ChangeListener {
public:
	EditorView(std::shared_ptr<midikraft::BCR2000> bcr);
	virtual ~EditorView() override;

	virtual void resized() override;
	void paintOverChildren(juce::Graphics& g) override;

	virtual void setRotaryParam(int knobNumber, TypedNamedValue *param) override;
	virtual void setButtonParam(int knobNumber, std::string const &name) override;

	// Get notified if the current synth changes
	virtual void changeListenerCallback(ChangeBroadcaster* source) override;

	// DragAndDropTarget implementation
	bool isInterestedInDragSource(const juce::DragAndDropTarget::SourceDetails& details) override;
	void itemDragEnter(const juce::DragAndDropTarget::SourceDetails& details) override;
	void itemDragMove(const juce::DragAndDropTarget::SourceDetails& details) override;
	void itemDragExit(const juce::DragAndDropTarget::SourceDetails& details) override;
	void itemDropped(const juce::DragAndDropTarget::SourceDetails& details) override;
	void dragOperationEnded(const juce::DragAndDropTarget::SourceDetails& details) override;

private:
	class UpdateSynthListener : public ValueTree::Listener {
	public:
		UpdateSynthListener(EditorView* papa);
		virtual ~UpdateSynthListener() override;

		void valueTreePropertyChanged(ValueTree& treeWhosePropertyHasChanged, const Identifier& property) override;
		void listenForMidiMessages(MidiInput* source, MidiMessage message);
		void updateAllKnobsFromPatch(std::shared_ptr<midikraft::Synth> synth, std::shared_ptr<midikraft::DataFile> newPatch);

	private:
		midikraft::MidiController::HandlerHandle midiHandler_ = midikraft::MidiController::makeOneHandle();
		std::shared_ptr<midikraft::DataFile> patch_;
		EditorView* papa_;
	};

	TypedNamedValueSet createParameterModel();
	std::shared_ptr<TypedNamedValue> findParameterByName(const juce::String& propertyName);
	void assignParameterToRotary(int rotaryIndex, std::shared_ptr<TypedNamedValue> param);
	void assignParameterToPress(int pressIndex, std::shared_ptr<TypedNamedValue> param);
	int rotaryIndexAt(juce::Point<int> localPos) const;
	int pressIndexAt(juce::Point<int> localPos) const;
	juce::Point<int> mousePositionInLocalSpace() const;
	void updateDropHoverState(const juce::DragAndDropTarget::SourceDetails& details);
	void clearDropHoverState();

	TypedNamedValueSet synthModel_;
	TypedNamedValueSet uiModel_;
	ValueTree uiValueTree_;
	TypedNamedValueSet controllerModel_;

	UpdateSynthListener updateSynthListener_;

	ValueTreeViewer valueTreeViewer_;
	OwnedArray<RotaryWithLabel> rotaryKnobs;
	OwnedArray<ToggleButton> pressKnobs;
	std::unique_ptr<LambdaButtonStrip> buttons_;
	std::shared_ptr<midikraft::BCR2000> bcr2000_;

	midikraft::Librarian librarian_;

	int hoveredRotaryIndex_ = -1;
	int hoveredPressIndex_ = -1;
};
