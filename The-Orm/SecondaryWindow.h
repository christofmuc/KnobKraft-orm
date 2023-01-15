/*
   Copyright (c) 2019-2023 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class SecondaryMainWindow : public juce::DocumentWindow
{
public:
	SecondaryMainWindow(std::string const& settingsName, int initialW, int initialH, Component* initialContent);

	void closeButtonPressed() override;

	void initialShow();
	void storeWindowState();

private:
	bool restoreWindowState();
	juce::ValueTree thisWindowSettings();

	std::string settingsName_;
};
