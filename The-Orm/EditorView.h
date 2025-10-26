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

class EditorView: public Component, public midikraft::BCR2000Proxy, private ChangeListener {
public:
	EditorView(std::shared_ptr<midikraft::BCR2000> bcr);
	virtual ~EditorView() override;

	virtual void resized() override;

	virtual void setRotaryParam(int knobNumber, TypedNamedValue *param) override;
	virtual void setButtonParam(int knobNumber, std::string const &name) override;

	// Get notified if the current synth changes
	virtual void changeListenerCallback(ChangeBroadcaster* source) override;

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
};

