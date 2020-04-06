/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "JuceHeader.h"

#include "LambdaButtonStrip.h"
#include "PropertyEditor.h"
#include "SynthHolder.h"
#include "Librarian.h"

class SettingsView : public Component {
public:
	SettingsView(std::vector<midikraft::SynthHolder> const &synths);
	virtual ~SettingsView() = default;

	void loadGlobals();

	virtual void resized() override;

private:
	std::vector<midikraft::SynthHolder> synths_;
	midikraft::Librarian librarian_;

	PropertyEditor propertyEditor_;
	LambdaButtonStrip buttonStrip_;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SettingsView)
};

