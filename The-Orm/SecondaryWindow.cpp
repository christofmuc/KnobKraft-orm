/*
   Copyright (c) 2019-2023 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "SecondaryWindow.h"

#include "Settings.h"

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
	setVisible(true);
	toFront(true);
}

void SecondaryMainWindow::closeButtonPressed() {
	storeWindowState();
	setVisible(false);
}

bool SecondaryMainWindow::restoreWindowState() {
	if (Settings::instance().keyIsSet(settingsKeyName())) {
		restoreWindowStateFromString(Settings::instance().get(settingsKeyName(), ""));
		return true;
	}
	else {
		return false;
	}
}

void SecondaryMainWindow::storeWindowState() {
	Settings::instance().set(settingsKeyName(), getWindowStateAsString().toStdString());
}

std::string SecondaryMainWindow::settingsKeyName() const
{
	return fmt::format("{}-Size", settingsName_);
}
