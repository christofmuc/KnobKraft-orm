/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "JuceHeader.h"

#include "BCR2000Proxy.h"

#include "BCR2000.h"
#include "LambdaButtonStrip.h"
#include "Librarian.h"

class RotaryWithLabel;
class SynthParameterDefinition;
class Synth;

class BCR2000_Component: public Component, public midikraft::BCR2000Proxy, private ChangeListener {
public:
	BCR2000_Component(std::shared_ptr<midikraft::BCR2000> bcr);
	virtual ~BCR2000_Component();

	virtual void resized() override;

	virtual void setRotaryParam(int knobNumber, TypedNamedValue *param) override;
	virtual void setButtonParam(int knobNumber, std::string const &name) override;

	// Get notified if the current synth changes
	virtual void changeListenerCallback(ChangeBroadcaster* source) override;

private:
	// The concept - we have three sets of parameters, all representing the current patch
	// The first set represents the synth. Modifying this will cause change messages sent out to the synth
	// The second set represents the external controller, e.g. the BCR2000. Modifying that will send updates to the controller so it will show the current state
	// The third set represents the UI. This is modified by moving UI sliders and will update sets one and three

	/*
	
	   Synth ----> sysex ---> MODEL 1 ---> MODEL 2 
	         <---- sysex ---- MODEL 1

			  Value Model 1   Value Model 2
			     var              var
	      --> Sysex to Synth


		  UI -> Sysex
		  Sysex -> UI
		  UI -> Controller
		  Controller -> UI
		  Controller -> Sysex

		  var
		  V1 -> send Sysex
		  V2 -> update Controller
		  V3 <-> refresh UI

	*/
	class UpdateSynthListener : public ValueTree::Listener {
	public:
		UpdateSynthListener(BCR2000_Component* papa);
		~UpdateSynthListener();

		void valueTreePropertyChanged(ValueTree& treeWhosePropertyHasChanged, const Identifier& property) override;
		void listenForMidiMessages(MidiInput* source, MidiMessage message);
		void updateAllKnobsFromPatch(std::shared_ptr<midikraft::DataFile> newPatch);

	private:
		midikraft::MidiController::HandlerHandle midiHandler_ = midikraft::MidiController::makeOneHandle();
		std::shared_ptr<midikraft::DataFile> patch_;
		BCR2000_Component* papa_;
	};

	class UpdateControllerListener : public ValueTree::Listener {
	public:
		UpdateControllerListener(BCR2000_Component* papa);
		~UpdateControllerListener();

		void listenForMidiMessages(MidiInput* source, MidiMessage message);
		void valueTreePropertyChanged(ValueTree& treeWhosePropertyHasChanged, const Identifier& property) override;

	private:
		midikraft::MidiController::HandlerHandle midiHandler_ = midikraft::MidiController::makeOneHandle();
		BCR2000_Component* papa_;
	};

	TypedNamedValueSet createParameterModel();

	TypedNamedValueSet synthModel_;
	TypedNamedValueSet uiModel_;
	ValueTree uiValueTree_;
	TypedNamedValueSet controllerModel_;

	UpdateSynthListener updateSynthListener_;
	UpdateControllerListener updateControllerListener_;

	OwnedArray<RotaryWithLabel> rotaryKnobs;
	OwnedArray<ToggleButton> pressKnobs;
	std::unique_ptr<LambdaButtonStrip> buttons_;
	std::shared_ptr<midikraft::BCR2000> bcr2000_;

	midikraft::Librarian librarian_;
};

