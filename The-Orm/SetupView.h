/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "JuceHeader.h"

//#include "HueLightControl.h"
#include "PropertyEditor.h"
#include "InfoText.h"
#include "LambdaButtonStrip.h"
#include "DebounceTimer.h"

#include "AutoDetection.h"
#include "SynthHolder.h"

#include "MidiChannelPropertyEditor.h"


class SetupView : public Component,
	private ChangeListener, private Value::Listener
{
public:
	SetupView(midikraft::AutoDetection *autoDetection /*, HueLightControl *lights*/);
	virtual ~SetupView() override;

	virtual void resized() override;
	
private:
	void refreshSynthActiveness();
	void refreshData();
	void rebuildSetupColumn();

	virtual void valueChanged(Value& value) override;
	virtual void changeListenerCallback(ChangeBroadcaster* source) override;
	void setValueWithoutListeners(Value &value, int newValue);
	
	std::shared_ptr<midikraft::SimpleDiscoverableDevice> findSynthForName(juce::String const &synthName) const;

	void quickConfigure();
	void loopDetection();
	void autoDetect();

	std::vector<midikraft::SynthHolder> sortedSynthList_;
	std::vector<std::shared_ptr<TypedNamedValue>> synths_;
	std::vector<std::shared_ptr<TypedNamedValue>> properties_;
	midikraft::AutoDetection *autoDetection_;
	InfoText header_;
	//HueLightControl * lights_;
	LambdaButtonStrip functionButtons_;
	TextButton autoConfigureButton_;
	PropertyEditor synthSelection_;
	PropertyEditor synthSetup_;

	DebounceTimer timedAction_;
};

