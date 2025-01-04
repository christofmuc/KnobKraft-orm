/*
   Copyright (c) 2019-2023 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "SecondaryWindow.h"

#include "UIModel.h"

#include <fmt/format.h>

SecondaryMainWindow::SecondaryMainWindow(std::string const& settingsName, int initialW, int initialH, Component* initialContent) 
	: juce::DocumentWindow("KnobKraft Quick Access", juce::Colours::black, juce::DocumentWindow::TitleBarButtons::allButtons, true)
	, settingsName_(settingsName)
{
	//setUsingNativeTitleBar(true);
	setResizable(true, false);
	setContentOwned(initialContent, false);
	if (!restoreWindowState()) {
		setSize(initialW, initialH);
	}
	if (thisWindowSettings().getProperty(PROPERTY_WINDOW_OPENNESS) == "1") {
		initialShow();
	}
}

void SecondaryMainWindow::initialShow() 
{
	setVisible(true);
	toFront(true);
	thisWindowSettings().setProperty(PROPERTY_WINDOW_OPENNESS, "1", nullptr);
}

juce::ValueTree SecondaryMainWindow::thisWindowSettings()
{
	auto windows = Data::instance().get().getOrCreateChildWithName(PROPERTY_WINDOW_LIST, nullptr);
	return windows.getOrCreateChildWithName(juce::Identifier(settingsName_), nullptr);
}

void SecondaryMainWindow::closeButtonPressed() {
	storeWindowState();
	setVisible(false);
	thisWindowSettings().setProperty(PROPERTY_WINDOW_OPENNESS, "0", nullptr);
}

bool SecondaryMainWindow::restoreWindowState() {
	if (thisWindowSettings().hasProperty(PROPERTY_WINDOW_SIZE)) {
		restoreWindowStateFromString(thisWindowSettings().getProperty(PROPERTY_WINDOW_SIZE));
		return true;
	}
	else {
		return false;
	}
}

void SecondaryMainWindow::storeWindowState() {
	thisWindowSettings().setProperty(PROPERTY_WINDOW_SIZE, getWindowStateAsString(), nullptr);
}

