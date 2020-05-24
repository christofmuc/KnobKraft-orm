/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "JuceHeader.h"

#include "BCR2000Proxy.h"

#include "BCR2000.h"
#include "LambdaButtonStrip.h"

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
	OwnedArray<RotaryWithLabel> rotaryKnobs;
	OwnedArray<ToggleButton> pressKnobs;
	std::unique_ptr<LambdaButtonStrip> buttons_;
	std::shared_ptr<midikraft::BCR2000> bcr2000_;
	void updateKnobValues();
};

