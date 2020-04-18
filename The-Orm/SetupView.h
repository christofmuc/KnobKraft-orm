/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "JuceHeader.h"

//#include "HueLightControl.h"
#include "MidiChannelEntry.h"
#include "LambdaButtonStrip.h"

#include "AutoDetection.h"

class SetupView : public Component,
	private ChangeListener
{
public:
	SetupView(midikraft::AutoDetection *autoDetection /*, HueLightControl *lights*/);

	virtual void resized() override;
	virtual void changeListenerCallback(ChangeBroadcaster* source) override;

	void refreshData();

private:
	std::string midiSetupDescription(std::shared_ptr<midikraft::SimpleDiscoverableDevice> synth) const;

	midikraft::AutoDetection *autoDetection_;
	//HueLightControl * lights_;
	LambdaButtonStrip functionButtons_;
	OwnedArray<Label> labels_;
	OwnedArray<Button> buttons_;
	OwnedArray<ColourSelector> colours_;
};

