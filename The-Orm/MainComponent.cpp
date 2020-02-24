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

};


//==============================================================================
MainComponent::MainComponent() :
	mainTabs_(TabbedButtonBar::Orientation::TabsAtTop),
	resizerBar_(&stretchableManager_, 1, false),
	logArea_(&logView_, BorderSize<int>(8)),
	midiLogArea_(&midiLogView_, BorderSize<int>(10))
{
	// Create the list of all synthesizers!
	std::vector<midikraft::SynthHolder>  synths;
	rev2_ = std::make_shared<midikraft::Rev2>();
	synths.push_back(midikraft::SynthHolder(std::dynamic_pointer_cast<midikraft::Synth>(rev2_), Colours::aqua));
	std::vector<std::shared_ptr<ActiveListItem>> listItems;
	for (auto s : synths) {
		listItems.push_back(std::make_shared<ActiveSynthHolder>(s.synth(), s.color()));
	}

	synthList_.setList(listItems, [this](std::shared_ptr<ActiveListItem> clicked) {});
	autodetector_.addChangeListener(&synthList_);

	// Create the patch view
	patchView_ = std::make_unique<PatchView>(synths);
	settingsView_ = std::make_unique<SettingsView>(synths);

	UIModel::instance()->currentSynth_.changeCurrentSynth(rev2_.get());

	// Setup the rest of the UI
	addAndMakeVisible(synthList_);
	mainTabs_.addTab("Library", Colours::black, patchView_.get(), true);
	mainTabs_.addTab("MIDI Log", Colours::black, &midiLogArea_, false);
	mainTabs_.addTab("Global Settings", Colours::black, settingsView_.get(), false);

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

	// Feel free to request the globals page from the Rev2
	settingsView_->loadGlobals();

	// Make sure you set the size of the component after
	// you add any child components.
	setSize(1536/2, 2048 / 2);
}

MainComponent::~MainComponent()
{
	Logger::setCurrentLogger(nullptr);
}

void MainComponent::resized()
{
	auto area = getLocalBounds();
	synthList_.setBounds(area.removeFromTop(60).reduced(8));
	//menuBar_.setBounds(area.removeFromTop(30));

	// make a list of two of our child components that we want to reposition
	Component* comps[] = { &mainTabs_, &resizerBar_, &logArea_ };

	// this will position the 3 components, one above the other, to fit
	// vertically into the rectangle provided.
	stretchableManager_.layOutComponents(comps, 3,
		area.getX(), area.getY(), area.getWidth(), area.getHeight(),
		true, true);
}

