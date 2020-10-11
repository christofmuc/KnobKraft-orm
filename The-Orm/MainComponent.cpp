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
#include "BCR2000.h"
#include "MKS80.h"

#include "GenericAdaption.h"


class ActiveSynthHolder : public midikraft::SynthHolder, public ActiveListItem {
public:
	ActiveSynthHolder(std::shared_ptr<midikraft::SimpleDiscoverableDevice> synth, Colour const &color) : midikraft::SynthHolder(synth, color) {
	}

	std::string getName() override
	{
		return synth() ? synth()->getName() : "Unnamed";
	}


	bool isActive() override
	{
		return device() ? device()->channel().isValid() : false;
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
	logger_ = std::make_unique<LogViewLogger>(logView_);
	database_ = std::make_unique<midikraft::PatchDatabase>();

	auto bcr2000 = std::make_shared <midikraft::BCR2000>();

	// Create the list of all synthesizers!
	std::vector<midikraft::SynthHolder>  synths;
	Colour buttonColour = getUIColour(LookAndFeel_V4::ColourScheme::UIColour::highlightedFill);
	synths.push_back(midikraft::SynthHolder(std::make_shared<midikraft::Matrix1000>(), buttonColour));
	//synths.push_back(midikraft::SynthHolder(std::make_shared<midikraft::KorgDW8000>(), buttonColour));
	synths.push_back(midikraft::SynthHolder(std::make_shared<midikraft::KawaiK3>(), buttonColour));
	synths.push_back(midikraft::SynthHolder(std::make_shared<midikraft::OB6>(), buttonColour));
	synths.push_back(midikraft::SynthHolder(std::make_shared<midikraft::Rev2>(), buttonColour));
	//synths.push_back(midikraft::SynthHolder(std::make_shared<midikraft::MKS80>(), buttonColour));
	synths.push_back(midikraft::SynthHolder(std::make_shared<midikraft::Virus>(), buttonColour));
	synths.push_back(midikraft::SynthHolder(std::make_shared<midikraft::RefaceDX>(), buttonColour));
	synths.push_back(midikraft::SynthHolder(bcr2000, buttonColour));

	// Now adding all adaptions
	auto adaptions = knobkraft::GenericAdaption::allAdaptions();
	for (auto adaption : adaptions) {
		synths.push_back(midikraft::SynthHolder(adaption, buttonColour));
	}

	UIModel::instance()->synthList_.setSynthList(synths);

	// Load activated state
	for (auto synth : synths) {
		if (!synth.device()) continue;
		auto activeKey = String(synth.device()->getName()) + String("-activated");
		// Check if the setting is set
		bool active = false;
		if (Settings::instance().keyIsSet(activeKey.toStdString())) {
			active = var(String(Settings::instance().get(activeKey.toStdString(), "1")));
		}
		else {
			// No user decision on active or not
			if (synth.device()->getName() != "Matrix 1000 Adaption") {
				// All synths except the example Matrix 1000 Adaption or turned on by default.
				// That one is turned off by default because there is a C++ implementation for the Matrix 1000 as well
				// and having both might confuse a first time user.
				active = true;
			}
		}
		UIModel::instance()->synthList_.setSynthActive(synth.device().get(), active);
		}

	refreshSynthList();

	autodetector_.addChangeListener(&synthList_);

	// Prepare for resizing the UI to fit on the screen
	auto globalScaling = (float)Desktop::getInstance().getDisplays().getMainDisplay().scale;
	setAcceptableGlobalScaleFactor();

	// Create the menu bar structure
	LambdaMenuModel::TMenuStructure menuStructure = {
		{0, { "File", { "Quit" } } },
		{1, { "MIDI", { "Auto-detect synths" } } },
		{2, { "Categories", { "Edit auto-categories", "Rerun auto categorize" } } },
		{3, { "View", { "Scale 100%", "Scale 125%", "Scale 150%", "Scale 175%", "Scale 200%" }}},
		{4, { "Help", { "Test Crash", "About" } } }
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
		int affected = database_->getPatchesCount(currentFilter);
		if (AlertWindow::showOkCancelBox(AlertWindow::QuestionIcon, "Re-run auto-categorization?",
			"Do you want to rerun the auto-categorization on the currently filtered " + String(affected) + " patches?\n\n"
			"This makes sense if you changed the auto category search strings!\n\n"
			"And don't worry, if you have manually set categories (or manually removed categories that were auto-detected), this information is retained!"
			)) {
			AutoCategorizeWindow window(database_.get(), getAutoCategoryFile().getFullPathName(), currentFilter, [this]() {
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
	}}},
	//, 0x51 /* Q */, ModifierKeys::ctrlModifier}}
	{ "Scale 100%", { 5, "Scale 100%", [this, globalScaling]() { Desktop::getInstance().setGlobalScaleFactor(1.0f / globalScaling); }}},
	{ "Scale 125%", { 6, "Scale 125%", [this, globalScaling]() { Desktop::getInstance().setGlobalScaleFactor(1.25f / globalScaling); }}},
	{ "Scale 150%", { 7, "Scale 150%", [this, globalScaling]() { Desktop::getInstance().setGlobalScaleFactor(1.5f / globalScaling); }}},
	{ "Scale 175%", { 8, "Scale 175%", [this, globalScaling]() { Desktop::getInstance().setGlobalScaleFactor(1.75f / globalScaling); }}},
	{ "Scale 200%", { 9, "Scale 200%", [this, globalScaling]() { Desktop::getInstance().setGlobalScaleFactor(2.0f / globalScaling); }}},
	{ "Test Crash", { 10, "Test Crash", [this]() {
		*((char *)(0)) = 1;
	}}},
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
	patchView_ = std::make_unique<PatchView>(*database_, synths);
	settingsView_ = std::make_unique<SettingsView>(synths);
	setupView_ = std::make_unique<SetupView>(&autodetector_);
	//recordingView_ = std::make_unique<RecordingView>(*patchView_);

	// Create Macro Definition view
	keyboardView_ = std::make_unique<KeyboardMacroView>([this](KeyboardMacroEvent event) {
		switch (event) {
		case KeyboardMacroEvent::Hide: patchView_->hideCurrentPatch(); break;
		case KeyboardMacroEvent::Favorite: patchView_->favoriteCurrentPatch(); break;
		case KeyboardMacroEvent::NextPatch: patchView_->selectNextPatch(); break;
		case KeyboardMacroEvent::PreviousPatch: patchView_->selectPreviousPatch(); break;
		case KeyboardMacroEvent::ImportEditBuffer: patchView_->retrieveEditBuffer(); break;
		default:
			SimpleLogger::instance()->postMessage("Error - invalid keyboard macro event detected");
			return;
		}
		SimpleLogger::instance()->postMessage("Keyboard Macro event fired " + KeyboardMacro::toText(event));
	});

	// Create the BCR2000 view, the predecessor to the generic editor view
	bcr2000View_ = std::make_unique<BCR2000_Component>(bcr2000);

	addAndMakeVisible(synthList_);
	Colour tabColour = getUIColour(LookAndFeel_V4::ColourScheme::UIColour::widgetBackground);
	mainTabs_.addTab("Library", tabColour, patchView_.get(), false);	
	//mainTabs_.addTab("Editor", tabColour, bcr2000View_.get(), false);
	//mainTabs_.addTab("Audio In", tabColour, recordingView_.get(), false);
	mainTabs_.addTab("MIDI Log", tabColour, &midiLogArea_, false);
	mainTabs_.addTab("Settings", tabColour, settingsView_.get(), false);
	mainTabs_.addTab("Macros", tabColour, keyboardView_.get(), false);
	mainTabs_.addTab("Setup", tabColour, setupView_.get(), false);

	addAndMakeVisible(mainTabs_);

	
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
			UIModel::instance()->currentSynth_.changeCurrentSynth(activeSynth);
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
	auto list = UIModel::instance()->synthList_.activeSynths();
	autodetector_.quickconfigure(list);

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

void MainComponent::setAcceptableGlobalScaleFactor() {
	// The idea is that we use a staircase of "good" scalings matching the Windows HighDPI settings of 100%, 125%, 150%, 175%, and 200%
	// and find out what is the largest scale factor that we still retain a virtual height of 1024 pixels (which is what I had designed this for at the start)
	float globalScaling = (float)Desktop::getInstance().getDisplays().getMainDisplay().scale;
	// So effectively, with a globalScaling of 1.0 (standard Windows normal DPI), this can make it only bigger, and with a Retina scaling factor 2.0 (Mac book pro) this can only shrink
	std::vector<float> scales = { 1.0f / globalScaling, 1.25f / globalScaling, 1.50f / globalScaling, 1.75f / globalScaling, 2.00f / globalScaling };
	auto availableHeight = Desktop::getInstance().getDisplays().getMainDisplay().userArea.getHeight();
	float goodScale = 1.0f;
	for (auto scale : scales) {
		if (availableHeight > 1024*scale) {
			goodScale = scale;
		}
	}
	Desktop::getInstance().setGlobalScaleFactor(goodScale);
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

void MainComponent::shutdown()
{
	// Shutdown database, which will make a backup
	database_.reset();
}

void MainComponent::refreshSynthList() {
	std::vector<std::shared_ptr<ActiveListItem>> listItems;
	for (auto s : UIModel::instance()->synthList_.allSynths()) {
		if (UIModel::instance()->synthList_.isSynthActive(s.device())) {
			listItems.push_back(std::make_shared<ActiveSynthHolder>(s.device(), s.color()));
		}
	}

	synthList_.setList(listItems, [this](std::shared_ptr<ActiveListItem> clicked) {
		auto activeSynth = std::dynamic_pointer_cast<ActiveSynthHolder>(clicked);
		if (activeSynth) {
			UIModel::instance()->currentSynth_.changeCurrentSynth(activeSynth->synth());
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
		StringArray tabnames = mainTabs_.getTabNames();
		for (int i = 0; i < mainTabs_.getNumTabs(); i++) {
			if (tabnames[i].endsWithIgnoreCase("settings")) {
				mainTabs_.setTabName(i, UIModel::currentSynth()->getName() + " settings");
				break;
			}
		}
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

