/*
   Copyright (c) 2019 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "MainComponent.h"

#include "Logger.h"
#include "MidiController.h"
#include "UIModel.h"

#include "AutoCategorizeWindow.h"
#include "AutoDetectProgressWindow.h"
#include "EditCategoryDialog.h"
#include "ExportDialog.h"

#include "OrmViews.h"

#include "Settings.h"

#include "PatchInterchangeFormat.h"

#include "LayoutConstants.h"

#ifndef _DEBUG
#ifdef USE_SENTRY
#include "sentry.h"
#endif
#endif

#ifdef USE_SPARKLE
#include "BinaryData.h"
#ifdef WIN32
#include <winsparkle.h>
#endif
#endif

//
class ActiveSynthHolder : public midikraft::SynthHolder, public ActiveListItem {
public:
	ActiveSynthHolder(std::shared_ptr<midikraft::SimpleDiscoverableDevice> synth, Colour const& color) : midikraft::SynthHolder(std::move(synth), color) {
	}

	std::string getName() override
	{
		return synth() ? synth()->getName() : "Unnamed";
	}


	bool isActive() override
	{
		return device() && device()->wasDetected();
	}


	Colour getColour() override
	{
		return color();
	}

};

Colour MainComponent::getUIColour(LookAndFeel_V4::ColourScheme::UIColour colourToGet) {
	auto lAF = &getLookAndFeel();
	auto v4 = dynamic_cast<LookAndFeel_V4*>(lAF);
	if (v4) {
		auto colorScheme = v4->getCurrentColourScheme();
		return colorScheme.getUIColour(colourToGet);
	}
	jassertfalse;
	return Colours::black;
}

//==============================================================================
MainComponent::MainComponent(DockManager& manager, DockManagerData& data, const juce::ValueTree& tree) :
	DockingWindow(manager, data, tree),
	globalScaling_(1.0f)
{
	setResizable(true, true);
	setUsingNativeTitleBar(true);

	refreshSynthList();

	// Prepare for resizing the UI to fit on the screen. Crash on headless devices
	globalScaling_ = (float)Desktop::getInstance().getDisplays().getPrimaryDisplay()->scale;
	float scale = calcAcceptableGlobalScaleFactor();
	auto persistedZoom = Settings::instance().get("zoom", "0");
	if (persistedZoom != "0") {
		auto stored = (float)atof(persistedZoom.c_str());
		if (stored >= 0.5f && stored <= 3.0f) {
			scale = stored;
		}
	}
	setZoomFactor(scale);

	menu_ = OrmViews::instance().getMainMenu();
	setMenuBar(menu_.get());

	UIModel::instance()->currentSynth_.addChangeListener(this);
	UIModel::instance()->synthList_.addChangeListener(this);

	// Is the active Synth persisted and active?
	std::string activeSynthName = Settings::instance().get("CurrentSynth", "");
	auto persistedSynth = UIModel::instance()->synthList_.synthByName(activeSynthName);
	if (persistedSynth.device() && UIModel::instance()->synthList_.isSynthActive(persistedSynth.device())) {
		UIModel::instance()->currentSynth_.changeCurrentSynth(persistedSynth.synth());
		//synthList_.setActiveListItem(activeSynthName);
	}
	else {
		// If at least one synth is enabled, use the first one!
		if (!UIModel::instance()->synthList_.activeSynths().empty()) {
			auto activeSynth = std::dynamic_pointer_cast<midikraft::Synth>(UIModel::instance()->synthList_.activeSynths()[0]);
			if (activeSynth) {
				UIModel::instance()->currentSynth_.changeCurrentSynth(activeSynth);
			}
		}
	}

	// Monitor the list of available MIDI devices
	midikraft::MidiController::instance()->addChangeListener(this);

	ormLookAndFeel_.setColourScheme(LookAndFeel_V4::getMidnightColourScheme());
	setLookAndFeel(&ormLookAndFeel_);
}

MainComponent::~MainComponent()
{
	UIModel::instance()->synthList_.removeChangeListener(this);
	UIModel::instance()->currentSynth_.removeChangeListener(this);

	setMenuBar(nullptr);

	Logger::setCurrentLogger(nullptr);

	setLookAndFeel(nullptr);
}

void MainComponent::setZoomFactor(float newZoomInPercentage) const {
	Desktop::getInstance().setGlobalScaleFactor(newZoomInPercentage / globalScaling_);
	Settings::instance().set("zoom", String(newZoomInPercentage).toStdString());
}

float MainComponent::calcAcceptableGlobalScaleFactor() {
	// The idea is that we use a staircase of "good" scaling factors matching the Windows HighDPI settings of 100%, 125%, 150%, 175%, and 200%
	// and find out what is the largest scale factor that we still retain a virtual height of 1024 pixels (which is what I had designed this for at the start)
	// So effectively, with a globalScaling of 1.0 (standard Windows normal DPI), this can make it only bigger, and with a Retina scaling factor 2.0 (Mac book pro) this can only shrink
	auto availableHeight = (float)Desktop::getInstance().getDisplays().getPrimaryDisplay()->userArea.getHeight();
	std::vector<float> scales = { 0.75f, 1.0f, 1.25f, 1.50f, 1.75f, 2.00f };
	float goodScale = 0.75f;
	for (auto scale : scales) {
		if (availableHeight > 1024 * scale) {
			goodScale = scale;
		}
	}
	return goodScale;
}

std::string MainComponent::getDatabaseFileName() const
{
	return OrmViews::instance().patchDatabase().getCurrentDatabaseFileName();
}

void MainComponent::refreshSynthList() {
	std::vector<std::shared_ptr<ActiveListItem>> listItems;
	std::vector<midikraft::PatchHolder> patchList;
	auto currentPatches = UIModel::instance()->currentPatch_.allCurrentPatches();
	for (auto s : UIModel::instance()->synthList_.allSynths()) {
		if (UIModel::instance()->synthList_.isSynthActive(s.device())) {
			listItems.push_back(std::make_shared<ActiveSynthHolder>(s.device(), s.color()));
			if (currentPatches.find(s.device()->getName()) != currentPatches.end()) {
				patchList.push_back(*currentPatches[s.device()->getName()]);
			}
			else {
				patchList.emplace_back(midikraft::PatchHolder());
			}
		}
	}

	// If the list of active synths was changed, it could be that the current synth no longer is an active synth. We need to do something about that!
	auto current = UIModel::currentSynth();
	if ((!current || (!UIModel::instance()->synthList_.isSynthActive(UIModel::instance()->synthList_.synthByName(current->getName()).device())))
		&& !listItems.empty()) {
		// Current synth is no longer active
		auto activeSynth = std::dynamic_pointer_cast<ActiveSynthHolder>(listItems[0]);
		UIModel::instance()->currentSynth_.changeCurrentSynth(activeSynth->synth());
	}
	else {
		if (listItems.empty()) {
			// No current synth anymore - turn off current synth
			UIModel::instance()->currentSynth_.changeCurrentSynth({});
		}
		else {
		}
	}

/*	synthList_.setList(listItems, [](std::shared_ptr<ActiveListItem> const& clicked) {
		auto activeSynth = std::dynamic_pointer_cast<ActiveSynthHolder>(clicked);
		if (activeSynth) {
			UIModel::instance()->currentSynth_.changeCurrentSynth(activeSynth->synth());
		}
		else {
			// What did you put into the list?
			jassert(false);
		}
		});

	// Need to make sure the correct button is pressed
	if (UIModel::currentSynth()) {
		synthList_.setActiveListItem(UIModel::currentSynth()->getName());
	}
	patchList_.setPatches(patchList);*/
}

void MainComponent::changeListenerCallback(ChangeBroadcaster* source)
{
	if (source == midikraft::MidiController::instance()) {
		// Kick off a new quickconfigure, as the MIDI interface setup has changed and synth available will be different
		auto synthList = UIModel::instance()->synthList_.activeSynths();
		quickconfigreDebounce_.callDebounced([this, synthList]() {
			auto myList = synthList;
			//autodetector_.quickconfigure(myList);
			}, 2000);
	}
	else if (source == &UIModel::instance()->synthList_) {
		// A synth has been activated or deactivated - rebuild the whole list at the top
		refreshSynthList();
		resized();
	}
	else if (dynamic_cast<CurrentSynth*>(source)) {
		auto synth = UIModel::currentSynth();
		if (synth) {
			// Persist current synth for next launch
			Settings::instance().set("CurrentSynth", synth->getName());
			// Make sure to let the synth list reflect the selection state!
			//synthList_.setActiveListItem(synth->getName());
		}


		// The active synth has been switched, make sure to refresh the tab name properly
		/*int index = findIndexOfTabWithNameEnding(&mainTabs_, "settings");
		if (index != -1) {
			// Rename tab to show settings of this synth
			if (UIModel::currentSynth()) {
				mainTabs_.setTabName(index, UIModel::currentSynth()->getName() + " settings");
			}
		}
		else {
			mainTabs_.setTabName(index, "Settings");
		}*/


		// The active synth has been switched, check if it is an adaptation and then refresh the adaptation view
		auto adaptation = std::dynamic_pointer_cast<knobkraft::GenericAdaptation>(UIModel::instance()->currentSynth_.smartSynth());
		Colour tabColour = getUIColour(LookAndFeel_V4::ColourScheme::UIColour::widgetBackground);
		/*if (adaptation) {
			//adaptationView_.setupForAdaptation(adaptation);
			int i = findIndexOfTabWithNameEnding(&mainTabs_, "Adaptation");
			if (i == -1) {
				// Need to add the tab back in
				//mainTabs_.addTab("Adaptation", tabColour, &adaptationView_, false, 2);
			}
			i = findIndexOfTabWithNameEnding(&mainTabs_, "settings");
			if (i != -1) {
				if (mainTabs_.getCurrentTabIndex() == i) {
					mainTabs_.setCurrentTabIndex(2);
				}
				mainTabs_.removeTab(i);
			}
		}
		else {
			int i = findIndexOfTabWithNameEnding(&mainTabs_, "Adaptation");
			if (i != -1) {
				int j = findIndexOfTabWithNameEnding(&mainTabs_, "settings");
				if (j == -1) {
					if (UIModel::currentSynth()) {
						//mainTabs_.addTab(UIModel::currentSynth()->getName() + " settings", tabColour, settingsView_.get(), false, 1);
					}
					else {
						//mainTabs_.addTab("Settings", tabColour, settingsView_.get(), false, 1);
					}
				}
				i = findIndexOfTabWithNameEnding(&mainTabs_, "Adaptation");
				if (mainTabs_.getCurrentTabIndex() == i) {
					mainTabs_.setCurrentTabIndex(i - 1);
				}
				mainTabs_.removeTab(i);
			}
		}*/
	}
}

int MainComponent::findIndexOfTabWithNameEnding(TabbedComponent* mainTabs, String const& name) {
	StringArray tabnames = mainTabs->getTabNames();
	for (int i = 0; i < mainTabs->getNumTabs(); i++) {
		if (tabnames[i].endsWithIgnoreCase(name)) {
			return i;
		}
	}
	return -1;
}

void MainComponent::aboutBox()
{
	String message = "This software is copyright 2020-2023 by Christof Ruch\n\n"
		"Released under dual license, by default under AGPL-3.0, but an MIT licensed version is available on request by the author\n"
		"\n"
		"This software is provided 'as-is,' without any express or implied warranty. In no event shall the author be held liable for any damages arising from the use of this software.\n"
		"\n"
		"Other licenses:\n"
		"This software is build using JUCE, who might want to track your IP address. See https://github.com/WeAreROLI/JUCE/blob/develop/LICENSE.md for details.\n"
		"The installer provided also contains the Microsoft Visual Studio 2017 Redistributable Package.\n"
		"\n"
		"Icons made by Freepik from www.flaticon.com\n"
		;
	AlertWindow::showMessageBox(AlertWindow::InfoIcon, "About", message, "Close");
}

void MainComponent::closeButtonPressed()
{
	Settings::instance().set("mainWindowSize", getWindowStateAsString().toStdString());

	if (juce::TopLevelWindow::getNumTopLevelWindows() == 1) {
		// Last top window was closed. Quit application (we're not on Mac)
		JUCEApplication::getInstance()->systemRequestedQuit();
	}
}
