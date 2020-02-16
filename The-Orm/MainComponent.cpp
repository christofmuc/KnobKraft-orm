/*
   Copyright (c) 2019 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "MainComponent.h"

#include "Logger.h"
#include "MidiController.h"
#include "UIModel.h"

#include "HorizontalLayoutContainer.h"

class LogViewLogger : public SimpleLogger {
public:
	LogViewLogger(LogView &logview) : logview_(logview) {}


	virtual void postMessage(const String& message) override
	{
		logview_.addMessageToList(message);
	}

private:
	LogView &logview_;
};


//==============================================================================
MainComponent::MainComponent() :
	mainTabs_(TabbedButtonBar::Orientation::TabsAtTop),
	resizerBar_(&stretchableManager_, 1, false),
	logArea_(new HorizontalLayoutContainer(&logView_, nullptr, -1, 0.0), BorderSize<int>(8)),
	buttons_(301, LambdaButtonStrip::Direction::Horizontal)
{
	LambdaButtonStrip::TButtonMap buttons = {
	{ "Detect", {0, "Detect", [this]() {
		detectBCR();
	}, 0x44 /* D */, ModifierKeys::ctrlModifier}},
	{ "Refresh preset list", {1, "Refresh preset list", [this]() {
		refreshFromBCR();
	}, 0x52 /* R */, ModifierKeys::ctrlModifier}},
	{ "Open", { 2, "Open", [this]() {
	}, 0x4F /* O */, ModifierKeys::ctrlModifier}},
	{ "Save", { 3, "Save", [this]() {
	}, 0x53 /* S */, ModifierKeys::ctrlModifier}},
	{ "Save as...", { 4, "Save as...", [this]() {
	}, 0x53 /* S */, ModifierKeys::ctrlModifier | ModifierKeys::altModifier}},
	{ "Send to BCR", { 5, "Send to BCR", [this]() {
	}, 0x0D /* ENTER */, ModifierKeys::ctrlModifier}},
	{ "Close", { 6, "Close", [this]() {
	}, 0x57 /* W */, ModifierKeys::ctrlModifier}},
	{ "About", { 7, "About", [this]() {
		aboutBox();
	}, -1, 0}},
	{ "Quit", { 8, "Quit", []() {
		JUCEApplicationBase::quit();
	}, 0x51 /* Q */, ModifierKeys::ctrlModifier}},
	};
	buttons_.setButtonDefinitions(buttons);
	commandManager_.registerAllCommandsForTarget(&buttons_);
	// Stop the focus-based search for the commands, and rather route all command implementation into this MainComponent
	// That was tricky to find out
	commandManager_.setFirstCommandTarget(this);
	addKeyListener(commandManager_.getKeyMappings());

	// Create the list of all synthesizers!
	std::vector<midikraft::SynthHolder>  synths;
	rev2_ = std::make_shared<midikraft::Rev2>();
	synths.push_back(midikraft::SynthHolder(std::dynamic_pointer_cast<midikraft::Synth>(rev2_), Colours::aqua));

	// Create the patch view
	patchView_ = std::make_unique<PatchView>(synths);

	UIModel::instance()->currentSynth_.changeCurrentSynth(rev2_.get());

	// Setup the rest of the UI
	mainTabs_.addTab("Library", Colours::black, patchView_.get(), true);
	mainTabs_.addTab("MIDI Log", Colours::black, &midiLogView_, false);

	addAndMakeVisible(mainTabs_);

	logger_ = std::make_unique<LogViewLogger>(logView_);
	addAndMakeVisible(menuBar_);
	addAndMakeVisible(resizerBar_);
	addAndMakeVisible(logArea_);

	// Resizer bar allows to enlarge the log area
	stretchableManager_.setItemLayout(0, -0.1, -0.9, -0.8); // The editor tab window prefers to get 80%
	stretchableManager_.setItemLayout(1, 5, 5, 5);  // The resizer is hard-coded to 5 pixels
	stretchableManager_.setItemLayout(2, -0.1, -0.9, -0.2);

	// Install our MidiLogger
	midikraft::MidiController::instance()->setMidiLogFunction([this](const MidiMessage& message, const String& source, bool isOut) {
		midiLogView_.addMessageToList(message, source, isOut);
	});

	// Do a quickconfigure
	std::vector<std::shared_ptr<midikraft::SimpleDiscoverableDevice>> synthForAutodetect;
	synthForAutodetect.push_back(rev2_);
	autodetector_.quickconfigure(synthForAutodetect);

	// Make sure you set the size of the component after
	// you add any child components.
	setSize(1280, 800);
}

MainComponent::~MainComponent()
{
	Logger::setCurrentLogger(nullptr);
}

void MainComponent::resized()
{
	auto area = getLocalBounds();
	//menuBar_.setBounds(area.removeFromTop(30));

	// make a list of two of our child components that we want to reposition
	Component* comps[] = { &mainTabs_, &resizerBar_, &logArea_ };

	// this will position the 3 components, one above the other, to fit
	// vertically into the rectangle provided.
	stretchableManager_.layOutComponents(comps, 3,
		area.getX(), area.getY(), area.getWidth(), area.getHeight(),
		true, true);
}

void MainComponent::refreshListOfPresets()
{
}

void MainComponent::detectBCR()
{
}

void MainComponent::refreshFromBCR()
{
}

void MainComponent::retrievePatch(int no)
{

}

juce::ApplicationCommandTarget* MainComponent::getNextCommandTarget()
{
	// Delegate to the lambda button strip
	return &buttons_;
}

void MainComponent::getAllCommands(Array<CommandID>& commands)
{
	// Editor itself has no commands, this is only used to delegate commands the CodeEditor does not handle to the lambda button strip
}

void MainComponent::getCommandInfo(CommandID commandID, ApplicationCommandInfo& result)
{
	// None, as no commands are registered here
}

bool MainComponent::perform(const InvocationInfo& info)
{
	// Always false, as no commands are registered here
	return false;
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
		;
	AlertWindow::showMessageBox(AlertWindow::InfoIcon, "About", message, "Close");
}

