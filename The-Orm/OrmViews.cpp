#include "OrmViews.h"

#include "SettingsView.h"
#include "SetupView.h"
#include "MainComponent.h"
#include "KeyboardMacroView.h"
#include "MidiLogView.h"
#include "AdaptationView.h"
#include "BCR2000_Component.h"
#include "PatchView.h"
#include "RecordingView.h"

#include "Settings.h"
#include "UIModel.h"

#include "Virus.h"
#include "Rev2.h"
#include "OB6.h"
#include "KorgDW8000.h"
#include "KawaiK3.h"
#include "Matrix1000.h"
#include "RefaceDX.h"
#include "BCR2000.h"
#include "MKS80.h"
#include "MKS50.h"

#include "GenericAdaptation.h"


class KnobKraftLogView: public LogView
{
public:
	KnobKraftLogView() {
		logger_ = std::make_unique<LogViewLogger>(*this);
	}

private:
	std::unique_ptr<LogViewLogger> logger_;
};

class KnobKraftMidiLog : public MidiLogView
{
public:
	KnobKraftMidiLog() {
		// Install our MidiLogger
		midikraft::MidiController::instance()->setMidiLogFunction([this](const MidiMessage& message, const String& source, bool isOut) {
			addMessageToList(message, source, isOut);
		});
	}

private:
	std::unique_ptr<LogViewLogger> logger_;
};

Colour getUIColour(LookAndFeel_V4::ColourScheme::UIColour colourToGet) {
	auto &lAF = juce::LookAndFeel::getDefaultLookAndFeel();
	auto v4 = dynamic_cast<LookAndFeel_V4*>(&lAF);
	if (v4) {
		auto colorScheme = v4->getCurrentColourScheme();
		return colorScheme.getUIColour(colourToGet);
	}
	jassertfalse;
	return Colours::black;
}

static std::shared_ptr<midikraft::BCR2000> s_BCR2000;


OrmViews::OrmViews() : librarian_({}) {
	//autodetector_.addChangeListener(UIModel::synthList_);

	auto customDatabase = Settings::instance().get("LastDatabase");
	File databaseFile(customDatabase);
	if (databaseFile.existsAsFile()) {
		database_ = std::make_unique<midikraft::PatchDatabase>(customDatabase, midikraft::PatchDatabase::OpenMode::READ_WRITE);
	}
	else {
		database_ = std::make_unique<midikraft::PatchDatabase>();
	}
	automaticCategories_ = database_->getCategorizer();

	// Create the list of all synthesizers!	
	std::vector<midikraft::SynthHolder>  synths;
	Colour buttonColour = getUIColour(LookAndFeel_V4::ColourScheme::UIColour::highlightedFill);
	synths.emplace_back(midikraft::SynthHolder(std::make_shared<midikraft::Matrix1000>(), buttonColour));
	synths.emplace_back(midikraft::SynthHolder(std::make_shared<midikraft::KorgDW8000>(), buttonColour));
	synths.emplace_back(midikraft::SynthHolder(std::make_shared<midikraft::KawaiK3>(), buttonColour));
	synths.emplace_back(midikraft::SynthHolder(std::make_shared<midikraft::OB6>(), buttonColour));
	synths.emplace_back(midikraft::SynthHolder(std::make_shared<midikraft::Rev2>(), buttonColour));
	synths.emplace_back(midikraft::SynthHolder(std::make_shared<midikraft::MKS50>(), buttonColour));
	synths.emplace_back(midikraft::SynthHolder(std::make_shared<midikraft::MKS80>(), buttonColour));
	synths.emplace_back(midikraft::SynthHolder(std::make_shared<midikraft::Virus>(), buttonColour));
	synths.emplace_back(midikraft::SynthHolder(std::make_shared<midikraft::RefaceDX>(), buttonColour));
	s_BCR2000 = std::make_shared<midikraft::BCR2000>();
	synths.emplace_back(midikraft::SynthHolder(s_BCR2000, buttonColour));

	// Now adding all adaptations
	auto adaptations = knobkraft::GenericAdaptation::allAdaptations();
	for (auto const& adaptation : adaptations) {
		synths.emplace_back(midikraft::SynthHolder(adaptation, buttonColour));
	}

	UIModel::instance()->synthList_.setSynthList(synths);

	// Load activated state
	for (auto synth : synths) {
		if (!synth.device()) continue;
		auto activeKey = String(synth.device()->getName()) + String("-activated");
		// Check if the setting is set
		bool active;
		if (Settings::instance().keyIsSet(activeKey.toStdString())) {
			active = var(String(Settings::instance().get(activeKey.toStdString(), "1")));
		}
		else {
			// No user decision on active or not - default is inactive now, else you end up with 20 synths which looks ugly
			active = false;
		}
		UIModel::instance()->synthList_.setSynthActive(synth.device().get(), active);
	}

}

OrmViews& OrmViews::instance() {
	if (!instance_) {
		instance_ = std::make_unique<OrmViews>();
	}
	return *instance_;
}

void OrmViews::shutdown() {
	instance_.reset();
}

midikraft::Librarian& OrmViews::librarian() {
	return instance().librarian_;
}

midikraft::AutoDetection& OrmViews::autoDetector() {
	return instance().autodetector_;
}

midikraft::PatchDatabase& OrmViews::patchDatabase() {
	return *database_;
}

std::shared_ptr<midikraft::AutomaticCategory> OrmViews::automaticCategories() {
	return automaticCategories_;
}


void OrmViews::reloadAutomaticCategories() {
	automaticCategories_ = database_->getCategorizer();
	UIModel::instance()->categoriesChanged.sendChangeMessage();
}

const juce::StringArray OrmViews::getAvailableViews() const 
{
	return {
			"Setup",
			"Settings",
			"Adaptation",
			"Macros",
			"Log",
			"MidiLog",
			"Recording",
			"BCR2000", 
			"Patch Library"
	};
}

std::shared_ptr<juce::Component> OrmViews::createView(const juce::String& nameOfViewToCreate) 
{
	if (nameOfViewToCreate == "Setup") {
		return std::make_shared<SetupView>(&autodetector_);
	}
	else if (nameOfViewToCreate == "Settings") {
		return std::make_shared<SettingsView>();
	}
	else if (nameOfViewToCreate == "Adaptation") {
		return std::make_shared<knobkraft::AdaptationView>();
	}
	else if (nameOfViewToCreate == "Macros") {
		return std::make_shared<KeyboardMacroView>([](KeyboardMacroEvent event) {
			ignoreUnused(event);
			});
	}
	else if (nameOfViewToCreate == "Log") {
		return std::make_shared<KnobKraftLogView>();
	}
	else if (nameOfViewToCreate == "MidiLog") {
		return std::make_shared<KnobKraftMidiLog>();
	}
	else if (nameOfViewToCreate == "Recording") {
		return std::make_shared<RecordingView>();
	}
	else if (nameOfViewToCreate == "BCR2000") {
		return std::make_shared<BCR2000_Component>(s_BCR2000);
	}
	else if (nameOfViewToCreate == "Patch Library") {
		return std::make_shared<PatchView>();
	}
	else if (nameOfViewToCreate == "" || nameOfViewToCreate == "root") {
		return nullptr;
	}
	else {
		jassertfalse;
		return nullptr;
	}
}

const juce::String OrmViews::getDefaultWindowName() const 
{
	return "KnobKraft Orm Docks Experiment";
}

std::shared_ptr<DockingWindow> OrmViews::createTopLevelWindow(DockManager& manager, DockManagerData& data, const juce::ValueTree& tree) {
	return std::make_shared<MainComponent>(manager, data, tree);
}

std::unique_ptr<OrmViews> OrmViews::instance_;
