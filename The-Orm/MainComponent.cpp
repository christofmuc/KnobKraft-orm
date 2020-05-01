/*
   Copyright (c) 2019 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "MainComponent.h"

#include "Logger.h"
#include "MidiController.h"
#include "UIModel.h"

#include "HorizontalLayoutContainer.h"

#include "AutoCategorizeWindow.h"
#include "AutoDetectProgressWindow.h"

#include "Settings.h"

#include "Virus.h"
#include "Rev2.h"
#include "OB6.h"
#include "KorgDW8000.h"
#include "KawaiK3.h"
#include "Matrix1000.h"
#include "RefaceDX.h"

#include "GenericAdaption.h"


class ActiveSynthHolder : public midikraft::SynthHolder, public ActiveListItem {
public:
	ActiveSynthHolder(std::shared_ptr<midikraft::Synth> synth, Colour const &color) : midikraft::SynthHolder(synth, color) {
	}

	std::string getName() override
	{
		return synth() ? synth()->getName() : "Unnamed";
	}


	bool isActive() override
	{
		return synth() ? synth()->channel().isValid() : false;
	}


	Colour getColour() override
	{
		return color();
	}

};

Colour MainComponent::getUIColour(LookAndFeel_V4::ColourScheme::UIColour colourToGet) {
	auto lAF = &getLookAndFeel();
	auto v4 = dynamic_cast<LookAndFeel_V4 *>(lAF);
	if (v4) {
		auto colorScheme = v4->getCurrentColourScheme();
		return colorScheme.getUIColour(colourToGet);
	}
	jassertfalse;
	return Colours::black;
}

//==============================================================================
MainComponent::MainComponent() :
	mainTabs_(TabbedButtonBar::Orientation::TabsAtTop),
	resizerBar_(&stretchableManager_, 1, false),
	logArea_(&logView_, BorderSize<int>(8)),
	midiLogArea_(&midiLogView_, BorderSize<int>(10)),
	buttons_(301)
{
	// Create the list of all synthesizers!
	std::vector<midikraft::SynthHolder>  synths;
	Colour buttonColour = getUIColour(LookAndFeel_V4::ColourScheme::UIColour::highlightedFill);
	synths.push_back(midikraft::SynthHolder(std::make_shared<midikraft::Matrix1000>(), buttonColour));
	synths.push_back(midikraft::SynthHolder(std::make_shared<midikraft::KorgDW8000>(), buttonColour));
	synths.push_back(midikraft::SynthHolder(std::make_shared<midikraft::KawaiK3>(), buttonColour));
	synths.push_back(midikraft::SynthHolder(std::make_shared<midikraft::OB6>(), buttonColour));
	synths.push_back(midikraft::SynthHolder(std::make_shared<midikraft::Rev2>(), buttonColour));
	synths.push_back(midikraft::SynthHolder(std::make_shared<midikraft::Virus>(), buttonColour));
	synths.push_back(midikraft::SynthHolder(std::make_shared<midikraft::RefaceDX>(), buttonColour));

	synths.push_back(midikraft::SynthHolder(std::make_shared<knobkraft::GenericAdaption>("PioneerToraiz-AS1"), buttonColour));

	UIModel::instance()->synthList_.setSynthList(synths);

	// Load activated state
	for (auto synth : synths) {
		if (!synth.device()) continue;
		auto activeKey = String(synth.device()->getName()) + String("-activated");
		auto active = var(String(Settings::instance().get(activeKey.toStdString(), "1")));
		UIModel::instance()->synthList_.setSynthActive(synth.device().get(), active);
	}

	refreshSynthList();
	
	autodetector_.addChangeListener(&synthList_);

	// Create the menu bar structure
	LambdaMenuModel::TMenuStructure menuStructure = {
		{0, { "File", { "Quit" } } },
		{1, { "MIDI", { "Auto-detect synths" } } },
		{2, { "Categories", { "Edit auto-categories", "Rerun auto categorize" } } },
		{3, { "Help", { "About" } } }
	};

	// Define the actions in the menu bar in form of an invisible LambdaButtonStrip 
	LambdaButtonStrip::TButtonMap buttons = {
	{ "Auto-detect synths", { 0, "Auto-detect synths", [this, synths]() {
		AutoDetectProgressWindow window(synths);
		window.runThread();
	} } },
	//}, 0x44 /* D */, ModifierKeys::ctrlModifier } },
	{ "Edit auto-categories", { 1, "Edit auto-categories", [this]() {
		if (!URL(getAutoCategoryFile().getFullPathName()).launchInDefaultBrowser()) {
			getAutoCategoryFile().revealToUser();
		}
	} } },
	{ "Rerun auto categorize...", { 2, "Rerun auto categorize", [this]() {
		auto currentFilter = patchView_->buildFilter();
		int affected = database_.getPatchesCount(currentFilter);
		if (AlertWindow::showOkCancelBox(AlertWindow::QuestionIcon, "Re-run auto-categorization?",
			"Do you want to rerun the auto-categorization on the currently filtered " + String(affected) + " patches?\n\n"
			"This makes sense if you changed the auto category search strings!\n\n"
			"And don't worry, if you have manually set categories (or manually removed categories that were auto-detected), this information is retained!"
			)) {
			AutoCategorizeWindow window(&database_, getAutoCategoryFile().getFullPathName(), currentFilter, [this]() {
				patchView_->retrieveFirstPageFromDatabase();
			});
			window.runThread();
		}
	} } },
	{ "About", { 3, "About", [this]() {
		aboutBox();
	}}},
	{ "Quit", { 4, "Quit", [this]() {
		JUCEApplicationBase::quit();
	}}}
	//, 0x51 /* Q */, ModifierKeys::ctrlModifier}}
	};
	buttons_.setButtonDefinitions(buttons);
	commandManager_.setFirstCommandTarget(&buttons_);
	commandManager_.registerAllCommandsForTarget(&buttons_);

	// Setup menu structure
	menuModel_ = std::make_unique<LambdaMenuModel>(menuStructure, &commandManager_, &buttons_);
	menuModel_->setApplicationCommandManagerToWatch(&commandManager_);
	menuBar_.setModel(menuModel_.get());
	addAndMakeVisible(menuBar_);

	// Create the patch view
	patchView_ = std::make_unique<PatchView>(database_, synths);
	settingsView_ = std::make_unique<SettingsView>(synths);
	setupView_ = std::make_unique<SetupView>(&autodetector_);

	// Create Macro Definition view
	keyboardView_ = std::make_unique<KeyboardMacroView>([this](KeyboardMacroEvent event) {
		switch (event) {
		case KeyboardMacroEvent::Hide: patchView_->hideCurrentPatch(); break;
		case KeyboardMacroEvent::Favorite: patchView_->favoriteCurrentPatch(); break;
		case KeyboardMacroEvent::NextPatch: patchView_->selectNextPatch(); break;
		case KeyboardMacroEvent::PreviousPatch: patchView_->selectPreviousPatch(); break;
		case KeyboardMacroEvent::ImportEditBuffer: patchView_->retrieveEditBuffer(); break;
		}
		SimpleLogger::instance()->postMessage("Keyboard Macro event fired " + KeyboardMacro::toText(event));
	});

	addAndMakeVisible(synthList_);
	Colour tabColour = getUIColour(LookAndFeel_V4::ColourScheme::UIColour::widgetBackground);
	mainTabs_.addTab("Library", tabColour, patchView_.get(), false);
	mainTabs_.addTab("MIDI Log", tabColour, &midiLogArea_, false);
	mainTabs_.addTab("Settings", tabColour, settingsView_.get(), false);
	mainTabs_.addTab("Macros", tabColour, keyboardView_.get(), false);
	mainTabs_.addTab("Setup", tabColour, setupView_.get(), false);

	addAndMakeVisible(mainTabs_);

	logger_ = std::make_unique<LogViewLogger>(logView_);
	addAndMakeVisible(menuBar_);
	addAndMakeVisible(resizerBar_);
	addAndMakeVisible(logArea_);

	UIModel::instance()->currentSynth_.addChangeListener(&synthList_);
	UIModel::instance()->currentSynth_.addChangeListener(this);
	UIModel::instance()->synthList_.addChangeListener(this);

	// If at least one synth is enabled, use the first one!
	if (UIModel::instance()->synthList_.activeSynths().size() > 0) {
		auto activeSynth = std::dynamic_pointer_cast<midikraft::Synth>(UIModel::instance()->synthList_.activeSynths()[0]);
		if (activeSynth) {
			UIModel::instance()->currentSynth_.changeCurrentSynth(activeSynth.get());
		}
	}

	// Setup the rest of the UI
	// Resizer bar allows to enlarge the log area
	stretchableManager_.setItemLayout(0, -0.1, -0.9, -0.8); // The editor tab window prefers to get 80%
	stretchableManager_.setItemLayout(1, 5, 5, 5);  // The resizer is hard-coded to 5 pixels
	stretchableManager_.setItemLayout(2, -0.1, -0.9, -0.2);

	// Install our MidiLogger
	midikraft::MidiController::instance()->setMidiLogFunction([this](const MidiMessage& message, const String& source, bool isOut) {
		midiLogView_.addMessageToList(message, source, isOut);
	});

	// Do a quickconfigure
	std::vector<std::shared_ptr<midikraft::SimpleDiscoverableDevice>> synthsForAutodetect;
	for (auto synth : synths) {
		auto device = std::dynamic_pointer_cast<midikraft::SimpleDiscoverableDevice>(synth.synth());
		if (device) {
			synthsForAutodetect.push_back(device);
		}
	}
	autodetector_.quickconfigure(synthsForAutodetect);

	// Feel free to request the globals page from the Rev2
	settingsView_->loadGlobals();

	// Make sure you set the size of the component after
	// you add any child components.
	setSize(1536/2, 2048 / 2);
}

MainComponent::~MainComponent()
{
	UIModel::instance()->synthList_.removeChangeListener(this);
	UIModel::instance()->currentSynth_.removeChangeListener(&synthList_);
	UIModel::instance()->currentSynth_.removeChangeListener(this);
	Logger::setCurrentLogger(nullptr);
}

void MainComponent::resized()
{
	auto area = getLocalBounds();
	menuBar_.setBounds(area.removeFromTop(LookAndFeel::getDefaultLookAndFeel().getDefaultMenuBarHeight()));
	auto topRow = area.removeFromTop(60).reduced(8);
	synthList_.setBounds(topRow);
	//menuBar_.setBounds(area.removeFromTop(30));

	// make a list of two of our child components that we want to reposition
	Component* comps[] = { &mainTabs_, &resizerBar_, &logArea_ };

	// this will position the 3 components, one above the other, to fit
	// vertically into the rectangle provided.
	stretchableManager_.layOutComponents(comps, 3,
		area.getX(), area.getY(), area.getWidth(), area.getHeight(),
		true, true);
}

void MainComponent::refreshSynthList() {
	std::vector<std::shared_ptr<ActiveListItem>> listItems;
	for (auto s : UIModel::instance()->synthList_.allSynths()) {
		if (UIModel::instance()->synthList_.isSynthActive(s.device())) {
			auto reallyASynth = s.synth();
			if (reallyASynth) {
				listItems.push_back(std::make_shared<ActiveSynthHolder>(reallyASynth, s.color()));
			}
		}
	}

	synthList_.setList(listItems, [this](std::shared_ptr<ActiveListItem> clicked) {
		auto activeSynth = std::dynamic_pointer_cast<ActiveSynthHolder>(clicked);
		if (activeSynth) {
			UIModel::instance()->currentSynth_.changeCurrentSynth(activeSynth->synth().get());
		}
		else {
			// What did you put into the list?
			jassert(false);
		}
	});
}

void MainComponent::changeListenerCallback(ChangeBroadcaster* source)
{
	if (source == &UIModel::instance()->synthList_) {
		// A synth has been activated or deactivated - rebuild the whole list at the top
		refreshSynthList();
	}
	else {
		// The active synth has been switched, make sure to refresh the tab name properly
		mainTabs_.setTabName(2, UIModel::currentSynth()->getName() + " settings");
	}
}

File MainComponent::getAutoCategoryFile() const {
	File appData = File::getSpecialLocation(File::userApplicationDataDirectory).getChildFile("KnobKraft");
	if (!appData.exists()) {
		appData.createDirectory();
	}
	File jsoncFile = appData.getChildFile("automatic_categories.jsonc");
	if (!jsoncFile.exists()) {
		// Create an initial file from the resources!
		FileOutputStream out(jsoncFile);
		out.writeText(midikraft::AutoCategory::defaultJson(), false, false, "\\n");
	}
	return jsoncFile;
}

void MainComponent::aboutBox()
{
	String message = "This software is copyright 2020 by Christof Ruch\n\n"
		"Released under dual license, by default under AGPL-3.0, but an MIT licensed version is available on request by the author\n"
		"\n"
		"This software is provided 'as-is,' without any express or implied warranty. In no event shall the author be held liable for any damages arising from the use of this software.\n"
		"\n"
		"Other licenses:\n"
		"This software is build using JUCE, who might want to track your IP address. See https://github.com/WeAreROLI/JUCE/blob/develop/LICENSE.md for details.\n"
		"The boost library is used for parts of this software, see https://www.boost.org/.\n"
		"The installer provided also contains the Microsoft Visual Studio 2017 Redistributable Package.\n"
		"\n"
		"Icons made by Freepik from www.flaticon.com\n"
		;
	AlertWindow::showMessageBox(AlertWindow::InfoIcon, "About", message, "Close");
}

