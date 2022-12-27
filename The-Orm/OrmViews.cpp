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

#include "EditCategoryDialog.h"
#include "AutoCategorizeWindow.h"
#include "PatchInterchangeFormat.h"

// Build a spdlog sink that goes into out standard log view
#include <spdlog/spdlog.h>
#include "SpdLogJuce.h"
#include <spdlog/sinks/base_sink.h>

// Some command name constants
const std::string kRetrievePatches{ "retrieveActiveSynthPatches" };
const std::string kFetchEditBuffer{ "fetchEditBuffer" };
const std::string kReceiveManualDump{ "receiveManualDump" };
const std::string kLoadSysEx{ "loadsysEx" };
const std::string kExportSysEx{ "exportSysex" };
const std::string kExportPIF{ "exportPIF" };
const std::string kShowDiff{ "showDiff" };
const std::string kSynthDetection{ "synthDetection" };
const std::string kLoopDetection{ "loopDetection" };
const std::string kSelectAdaptationDirect{ "selectAdaptationDir" };
const std::string kCreateNewAdaptation{ "createNewAdaptation" };
const std::string kLoadTemplate{ "loadViewTemplate" };
const std::string kSaveTemplate{ "saveViewTemplate" };



template<typename Mutex>
class LogViewSink : public spdlog::sinks::base_sink<Mutex>
{
public:
	LogViewSink(LogView& logView) : logView_(logView) {}

protected:
	void sink_it_(const spdlog::details::log_msg& msg) override
	{
		spdlog::memory_buf_t formatted;
		this->formatter_->format(msg, formatted);
		logView_.logMessage(msg.level, fmt::to_string(formatted));
	}

	void flush_() override
	{
		// NOP
	}

private:
	LogView& logView_;
};

#include <mutex>
using LogViewSink_mt = LogViewSink<std::mutex>;


class KnobKraftLogView : public LogView
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

OrmDockableWindow::OrmDockableWindow() {
	// Select colour scheme
	ormLookAndFeel_.setColourScheme(LookAndFeel_V4::getMidnightColourScheme());
	setLookAndFeel(&ormLookAndFeel_);
}


OrmDockableWindow::~OrmDockableWindow() {
	setLookAndFeel(nullptr);
}


Colour getUIColour(LookAndFeel_V4::ColourScheme::UIColour colourToGet) {
	auto& lAF = juce::LookAndFeel::getDefaultLookAndFeel();
	auto v4 = dynamic_cast<LookAndFeel_V4*>(&lAF);
	if (v4) {
		auto colorScheme = v4->getCurrentColourScheme();
		return colorScheme.getUIColour(colourToGet);
	}
	jassertfalse;
	return Colours::black;
}

static std::shared_ptr<midikraft::BCR2000> s_BCR2000;


OrmViews::OrmViews() : librarian_({}), buttons_(301)
{
	//autodetector_.addChangeListener(UIModel::synthList_);
	// 
	// Setup spd logging
	std::vector<spdlog::sink_ptr> sinks;
	logView_ = std::make_shared<KnobKraftLogView>();
	sinks.push_back(std::make_shared<LogViewSink_mt>(*logView_));
	spdLogger_ = std::make_shared<spdlog::logger>("KnobKraftOrm", begin(sinks), end(sinks));
	spdLogger_->set_pattern("%H:%M:%S: %l %v");
	spdlog::set_default_logger(spdLogger_);
	spdlog::flush_every(std::chrono::milliseconds(50));
	spdlog::set_level(spdlog::level::debug);
	spdlog::info("Launching KnobKraft Orm");

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

	// Pre-create some views
	setupView_ = std::make_shared<SetupView>(&autodetector_);
	midiLogView_ = std::make_shared<KnobKraftMidiLog>();

	// Setup menus
	recentFiles_.setMaxNumberOfItems(10);
	if (Settings::instance().keyIsSet("RecentFiles")) {
		recentFiles_.restoreFromString(Settings::instance().get("RecentFiles"));
	}

	// Create the menu bar structure
	LambdaMenuModel::TMenuStructure menuStructure = {
		{0, { "File", {
				{ "New database..." },
				{ "Open database..." },
				{ "Save database as..." },
				{ "Open recent...", true, 3333, [this]() {  return recentFileMenu(); }, [this](int selected) {  recentFileSelected(selected); }  },
				{ "Export multiple databases..."  },
				{ "Merge multiple databases..."  },
				{ "Quit" } } } },
		{1, { "Edit", { { "Copy patch to clipboard..." },  { "Bulk rename patches..."},  {"Delete patches..."}, {"Reindex patches..."}}}},
		{2, { "MIDI", { { "Auto-detect synths" }, { kSynthDetection},  { kRetrievePatches }, { kFetchEditBuffer }, { kReceiveManualDump }, { kLoopDetection} }}},
		{3, { "Patches", { { kLoadSysEx}, { kExportSysEx }, { kExportPIF}, { kShowDiff} }}},
		{4, { "Categories", { { "Edit categories" }, {{ "Show category naming rules file"}},  {"Edit category import mapping"},  {"Rerun auto categorize"}}}},
		{5, { "View", { 
			{ "Open new view...", true, 4461, [this]()
				{
					return newViewMenu();
				}, [this](int selected) {
					newViewSelected(selected);
				}
			},
			{ "Load view layout...", true, 4444, [this]()
				{
					return loadLayoutMenu();
				}, [this](int selected) {
					layoutTemplateSelected(selected);
				} 
			},
			{ kSaveTemplate },
			{"Scale 75%"}, {"Scale 100%"}, {"Scale 125%"}, {"Scale 150%"}, {"Scale 175%"}, {"Scale 200%"}}}},
		{6, { "Options", { { kCreateNewAdaptation}, { kSelectAdaptationDirect} }}},
		{7, { "Help", {
#ifndef _DEBUG
#ifdef USE_SENTRY
			{ "Crash software.."},
			{ "Crash reporting consent" },
#endif
#endif
			{ "Check for updates..." },
			{ "About" } } } }
	};

	// Define the actions in the menu bar in form of an invisible LambdaButtonStrip 
	LambdaButtonStrip::TButtonMap buttons = {
	{ "Auto-detect synths", { "Auto-detect synths", [this]() {
		OrmViews::instance().setupView()->autoDetect();
	}, juce::KeyPress::F1Key } },
	{ "Import patches from synth", { kRetrievePatches, [this]() {
		activePatchView().retrievePatches();
	}, juce::KeyPress::F7Key } },
	{ "Import edit buffer from synth",{ kFetchEditBuffer, [this]() {
		activePatchView().retrieveEditBuffer();
	}, juce::KeyPress::F8Key  } },
	{ "Receive manual dump",{ kReceiveManualDump, [this]() {
		activePatchView().receiveManualDump();
	}, juce::KeyPress::F9Key  } },
	{ "Import from files into database", { kLoadSysEx, [this]() {
		activePatchView().loadPatches();
	}, juce::KeyPress::F3Key } },
	{ "Export into sysex files", { kExportSysEx , [this]() {
		activePatchView().exportPatches();
	}}},
	{ "Export into PIF", { kExportPIF, [this]() {
		activePatchView().createPatchInterchangeFile();
	} } },
	{ "Show patch comparison", { kShowDiff , [this]() {
		activePatchView().showPatchDiffDialog();
	} } },
	{ "Quick check connectivity", { kSynthDetection, [this]() {
		OrmViews::instance().setupView()->quickConfigure();
	}, juce::KeyPress::F2Key } },
	{ "Check for MIDI loops", { kLoopDetection, [this]() {
		OrmViews::instance().setupView()->loopDetection();
	} } },
	{"Set User Adaptation Dir", { kSelectAdaptationDirect, [this]() {
		FileChooser directoryChooser("Please select the directory to store your user adaptations...", File(knobkraft::GenericAdaptation::getAdaptationDirectory()));
		if (directoryChooser.browseForDirectory()) {
			knobkraft::GenericAdaptation::setAdaptationDirectoy(directoryChooser.getResult().getFullPathName().toStdString());
			juce::AlertWindow::showMessageBox(AlertWindow::InfoIcon, "Restart required", "Your new adaptations directory will only be used after a restart of the application!");
		}
	} } },
	{"Create new adaptation", { kCreateNewAdaptation, [this]() {
		OrmViews::instance().setupView()->createNewAdaptation();
	} } },

	{"Save current view layout", { kSaveTemplate, [this]() {
		saveLayoutTemplate();
	} } },
	{ "Edit categories", { "Edit categories", [this]() {
		EditCategoryDialog::showEditDialog(OrmViews::instance().patchDatabase(), activeMainWindow(), [this](std::vector<midikraft::CategoryDefinition> const& newDefinitions) {
			OrmViews::instance().patchDatabase().updateCategories(newDefinitions);
			// Need to reload the automatic Categories!
			OrmViews::instance().reloadAutomaticCategories();

		});
	} } },
	{ "Show category naming rules file", { "Show category naming rules file", [this]() {
		// This will create the file on demand, copying out the built-in information!
		if (!URL(OrmViews::instance().automaticCategories()->getAutoCategoryFile().getFullPathName()).launchInDefaultBrowser()) {
			OrmViews::instance().automaticCategories()->getAutoCategoryFile().revealToUser();
		}
	} } },
	{ "Edit category import mapping", { "Edit category import mapping", [this]() {
		// This will create the file on demand, copying out the built-in information!
		if (!URL(OrmViews::instance().automaticCategories()->getAutoCategoryMappingFile().getFullPathName()).launchInDefaultBrowser()) {
			OrmViews::instance().automaticCategories()->getAutoCategoryMappingFile().revealToUser();
		}
	} } },
	{ "Rerun auto categorize...", { "Rerun auto categorize", [this]() {
		auto currentFilter = activePatchView().currentFilter();
		int affected = OrmViews::instance().patchDatabase().getPatchesCount(currentFilter);
		if (AlertWindow::showOkCancelBox(AlertWindow::QuestionIcon, "Re-run auto-categorization?",
			"Do you want to rerun the auto-categorization on the currently filtered " + String(affected) + " patches?\n\n"
			"This makes sense if you changed the auto category search strings, or the import mappings!\n\n"
			"And don't worry, if you have manually set categories (or manually removed categories that were auto-detected), this information is retained!"
			)) {
			OrmViews::instance().reloadAutomaticCategories();
			AutoCategorizeWindow window(OrmViews::instance().patchDatabase(), OrmViews::instance().automaticCategories(), currentFilter, [this]() {
				activePatchView().retrieveFirstPageFromDatabase();
			});
			window.runThread();
		}
	} } },
	{ "About", { "About", []() {
		MainComponent::aboutBox();
	}}},
	{ "Quit", { "Quit", []() {
		JUCEApplicationBase::quit();
	}}},
	//, 0x51 /* Q */, ModifierKeys::ctrlModifier}}
//	{ "Scale 75%", { "Scale 75%", [this]() { setZoomFactor(0.75f); }}},
//	{ "Scale 100%", { "Scale 100%", [this]() { setZoomFactor(1.0f); }}},
//	{ "Scale 125%", { "Scale 125%", [this]() { setZoomFactor(1.25f); }}},
//	{ "Scale 150%", { "Scale 150%", [this]() { setZoomFactor(1.5f); }}},
//	{ "Scale 175%", { "Scale 175%", [this]() { setZoomFactor(1.75f); }}},
//	{ "Scale 200%", { "Scale 200%", [this]() { setZoomFactor(2.0f); }}},
	{ "New database...", { "New database...", [this] {
		createNewDatabase();
	}}},
	{ "Open database...", { "Open database...", [this] {
		openDatabase();
	}}},
	{ "Save database as...", { "Save database as...", [this] {
		saveDatabaseAs();
	}}},
	{ "Export multiple databases...", { "Export multiple databases...", []() {
		exportDatabases();
	}}},
	{ "Merge multiple databases...", { "Merge multiple databases...", [this]() {
		mergeDatabases();
	}}},
	{ "Bulk rename patches...", { "Bulk rename patches...", [this] {
		activePatchView().bulkRenamePatches();
	}}},
	{ "Delete patches...", { "Delete patches...", [this] {
		activePatchView().deletePatches();
	}}},
	{ "Reindex patches...", { "Reindex patches...", [this] {
		activePatchView().reindexPatches();
	}}},
	{ "Copy patch to clipboard...", { "Copy patch to clipboard...", [] {
		auto patch = UIModel::currentPatch();
		if (patch.patch() && patch.synth()) {
			std::stringstream buffer;
			buffer << "\"sysex\" : ["; // This is the very specific CF Sysex format
			bool first = true;
			auto messages = patch.synth()->dataFileToSysex(patch.patch(), nullptr);
			for (const auto& m : messages) {
				for (int i = 0; i < m.getRawDataSize(); i++) {
					if (!first) {
						buffer << ", ";
					}
					else {
						first = false;
					}
					buffer << (int)m.getRawData()[i];
				}
			}
			buffer << "]";
			SystemClipboard::copyTextToClipboard(buffer.str());
		}
	}}},
#ifndef _DEBUG
#ifdef USE_SENTRY
	{ "Crash reporting consent...", { "Crash reporting consent", [this] {
		checkUserConsent();
	}}},
	{ "Crash software...", { "Crash software..", [this] {
		crashTheSoftware();
	}}},
#endif 
#endif
#ifdef USE_SPARKLE
		{ "Check for updates...", { "Check for updates...", [this] {
#ifdef WIN32
			win_sparkle_check_update_with_ui();
#endif
		}}},
#endif
	};
	buttons_.setButtonDefinitions(buttons);
	commandManager_.setFirstCommandTarget(&buttons_);
	commandManager_.registerAllCommandsForTarget(&buttons_);
	//TODOgetTopLevelComponent()->addKeyListener(commandManager_.getKeyMappings());

	// Setup menu structure
	menuModel_ = std::make_unique<LambdaMenuModel>(menuStructure, &commandManager_, &buttons_);
	menuModel_->setApplicationCommandManagerToWatch(&commandManager_);

	// Refresh Window title and other things to do when the MainComponent is displayed
	MessageManager::callAsync([this]() {
		UIModel::instance()->windowTitle_.sendChangeMessage();
#ifdef WIN32
		checkForUpdatesOnStartup();
#endif
		});
}

OrmViews::~OrmViews() {
	menuModel_.reset();
}

OrmViews& OrmViews::instance() {
	if (!instance_) {
		instance_ = std::make_unique<OrmViews>();
	}
	return *instance_;
}

void OrmViews::shutdown() {
	instance_->menuModel_->setApplicationCommandManagerToWatch(nullptr);
	instance_.reset();
}

void OrmViews::init(DockManager* dockManager) {
	dockManager_ = dockManager;
}

std::shared_ptr<juce::MenuBarModel> OrmViews::getMainMenu() {
	return menuModel_;
}

Component* OrmViews::activeMainWindow() {
	jassert(dockManager_);
	Component *currentWindow = dockManager_->getCurrentlyFocusedComponent();
	do {
		DockingWindow* topLevel = dynamic_cast<DockingWindow*>(currentWindow);
		if (topLevel) {
			return topLevel;
		}
		if (currentWindow) {
			currentWindow = currentWindow->getParentComponent();
		}
	} while (currentWindow);
	return nullptr;
}

PatchView& OrmViews::activePatchView() {
	if (patchViews_.empty()) {
		patchViews_.push_back(std::make_shared<PatchView>());
	}
	return *patchViews_[0];
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
			"Patch Library",
			"Current Patch",
			"Synth Bank"
	};
}

bool OrmViews::canCreateAdditionalViews(juce::String const& name) {
	if (name == "Setup" || name == "Macros" || name == "MidiLog" || name == "Log") {
		return false;
	}
	return true;
}

std::shared_ptr<juce::Component> OrmViews::createView(const juce::String& nameOfViewToCreate)
{
	if (nameOfViewToCreate == "Setup") {
		return setupView_;
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
		return logView_;
	}
	else if (nameOfViewToCreate == "MidiLog") {
		return midiLogView_;
	}
	else if (nameOfViewToCreate == "Recording") {
		return std::make_shared<RecordingView>();
	}
	else if (nameOfViewToCreate == "BCR2000") {
		return std::make_shared<BCR2000_Component>(s_BCR2000);
	}
	else if (nameOfViewToCreate == "Patch Library") {
		auto newPatchView = std::make_shared<PatchView>();
		patchViews_.push_back(newPatchView);
		return newPatchView;
	}
	else if (nameOfViewToCreate == "Current Patch") {
		auto currentPatchDisplay = std::make_shared<CurrentPatchDisplay>(PatchView::predefinedCategories(),
			[this](std::shared_ptr<midikraft::PatchHolder> favoritePatch) {
				OrmViews::instance().patchDatabase().putPatch(*favoritePatch);
				//patchButtons_->refresh(true);
			}
		);
		currentPatchDisplay->onCurrentPatchClicked = [this](std::shared_ptr<midikraft::PatchHolder> patch) {
			if (patch) {
				//selectPatch(*patch, true);
			}
		};
		return currentPatchDisplay;
	}
	else if (nameOfViewToCreate == "Synth Bank") {
		auto synthBank = std::make_shared<SynthBankPanel>(OrmViews::instance().patchDatabase());
		return synthBank;
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

void OrmViews::checkForUpdatesOnStartup() {
#ifdef USE_SPARKLE
#ifdef WIN32
	win_sparkle_set_appcast_url("https://raw.githubusercontent.com/christofmuc/appcasts/master/KnobKraft-Orm/appcast.xml");
	win_sparkle_set_dsa_pub_pem(BinaryData::dsa_pub_pem);
	win_sparkle_set_error_callback(logSparkleError);
	win_sparkle_set_shutdown_request_callback(sparkleInducedShutdown);
	win_sparkle_init();
#endif
#endif
}

void OrmViews::createNewDatabase()
{
	std::string databasePath = Settings::instance().get("LastDatabasePath");
	FileChooser databaseChooser("Please enter the name of the database file to create...", File(databasePath), "*.db3");
	if (databaseChooser.browseForFileToSave(true)) {
		File databaseFile(databaseChooser.getResult());
		if (databaseFile.existsAsFile()) {
			if (!AlertWindow::showOkCancelBox(AlertWindow::WarningIcon, "File already exists", "The file will be overwritten and all contents will be lost! Do you want to proceed?")) {
				return;
			}
			databaseFile.deleteFile();
		}
		recentFiles_.addFile(File(OrmViews::instance().patchDatabase().getCurrentDatabaseFileName()));
		if (OrmViews::instance().patchDatabase().switchDatabaseFile(databaseFile.getFullPathName().toStdString(), midikraft::PatchDatabase::OpenMode::READ_WRITE)) {
			persistRecentFileList();
			// That worked, new database file is in use!
			Settings::instance().set("LastDatabasePath", databaseFile.getParentDirectory().getFullPathName().toStdString());
			Settings::instance().set("LastDatabase", databaseFile.getFullPathName().toStdString());
			// Refresh UI
			UIModel::instance()->currentSynth_.sendChangeMessage();
			UIModel::instance()->windowTitle_.sendChangeMessage();
			UIModel::instance()->databaseChanged.sendChangeMessage();
		}
	}
}

void OrmViews::openDatabase()
{
	std::string lastPath = Settings::instance().get("LastDatabasePath", "");
	if (lastPath.empty()) {
		lastPath = File(midikraft::PatchDatabase::generateDefaultDatabaseLocation()).getParentDirectory().getFullPathName().toStdString();
	}
	File lastDirectory(lastPath);
	FileChooser databaseChooser("Please select a KnobKraft Orm SQlite database file...", lastDirectory, "*.db3");
	if (databaseChooser.browseForFileToOpen()) {
		File databaseFile = databaseChooser.getResult();
		openDatabase(databaseFile);
	}
}

void OrmViews::openDatabase(File& databaseFile)
{
	if (databaseFile.existsAsFile()) {
		recentFiles_.addFile(File(OrmViews::instance().patchDatabase().getCurrentDatabaseFileName()));
		if (OrmViews::instance().patchDatabase().switchDatabaseFile(databaseFile.getFullPathName().toStdString(), midikraft::PatchDatabase::OpenMode::READ_WRITE)) {
			recentFiles_.removeFile(databaseFile);
			persistRecentFileList();
			// That worked, new database file is in use!
			Settings::instance().set("LastDatabasePath", databaseFile.getParentDirectory().getFullPathName().toStdString());
			Settings::instance().set("LastDatabase", databaseFile.getFullPathName().toStdString());
			// Refresh UI
			UIModel::instance()->currentSynth_.sendChangeMessage();
			UIModel::instance()->windowTitle_.sendChangeMessage();
			UIModel::instance()->databaseChanged.sendChangeMessage();
		}
	}
}

void OrmViews::saveDatabaseAs()
{
	std::string lastPath = Settings::instance().get("LastDatabasePath", "");
	if (lastPath.empty()) {
		lastPath = File(midikraft::PatchDatabase::generateDefaultDatabaseLocation()).getParentDirectory().getFullPathName().toStdString();
	}
	File lastDirectory(lastPath);
	FileChooser databaseChooser("Please choose a new KnobKraft Orm SQlite database file...", lastDirectory, "*.db3");
	if (databaseChooser.browseForFileToSave(true)) {
		File databaseFile = databaseChooser.getResult();
		OrmViews::instance().patchDatabase().makeDatabaseBackup(databaseFile);
		openDatabase(databaseFile);
	}
}

class MergeAndExport : public ThreadWithProgressWindow {
public:
	explicit MergeAndExport(Array<File> databases) : ThreadWithProgressWindow("Exporting databases...", true, true), databases_(std::move(databases)) {
	}

	void run() override
	{
		// Build synth list
		std::vector<std::shared_ptr<midikraft::Synth>> allSynths;
		for (auto& synth : UIModel::instance()->synthList_.allSynths()) {
			allSynths.push_back(synth.synth());
		}

		double done = 0.0f;
		int count = 0;
		for (auto const& file : databases_) {
			if (threadShouldExit()) {
				break;
			}
			File exported = file.withFileExtension(".json");
			if (!exported.exists()) {
				std::vector<midikraft::PatchHolder> allPatches;
				try {
					midikraft::PatchDatabase mergeSource(file.getFullPathName().toStdString(), midikraft::PatchDatabase::OpenMode::READ_ONLY);
					midikraft::PatchFilter filter(allSynths);
					spdlog::info("Exporting database file {} containing {} patches", file.getFullPathName(), mergeSource.getPatchesCount(filter));
					allPatches = mergeSource.getPatches(filter, 0, -1);
				}
				catch (midikraft::PatchDatabaseReadonlyException& e) {
					ignoreUnused(e);
					// This exception is thrown when opening the database caused a write operation. Most likely this is an old database needing to run migration code first.
					// We'll do this by creating a backup as temporary file.
					File tempfile = File::createTempFile("db3");
					try {
						midikraft::PatchDatabase::makeDatabaseBackup(file, tempfile);
						midikraft::PatchDatabase mergeSource(tempfile.getFullPathName().toStdString(), midikraft::PatchDatabase::OpenMode::READ_WRITE_NO_BACKUPS);
						midikraft::PatchFilter filter(allSynths);
						spdlog::info("Exporting database file {} containing {} patches", file.getFullPathName(), mergeSource.getPatchesCount(filter));
						allPatches = mergeSource.getPatches(filter, 0, -1);
					}
					catch (midikraft::PatchDatabaseException& e) {
						spdlog::error("Fatal error opening database file {}: {}", file.getFullPathName(), e.what());
					}
					tempfile.deleteFile();
				}
				catch (midikraft::PatchDatabaseException& e) {
					spdlog::error("Fatal error opening database file {}: {}", file.getFullPathName(), e.what());
				}

				// We have all patches in memory - write them into a pip file
				if (!allPatches.empty()) {
					midikraft::PatchInterchangeFormat::save(allPatches, exported.getFullPathName().toStdString());
				}
			}
			else {
				spdlog::warn("Not exporting because file already exists: {}", exported.getFullPathName());
			}

			done += 1.0;
			setProgress(done / databases_.size());
			count++;
		}
		spdlog::info("Done, exported {} databases to pip files for reimport and merge", count);
	}

private:
	Array<File> databases_;

};

void OrmViews::exportDatabases()
{
	std::string lastPath = Settings::instance().get("LastDatabaseMergePath", "");
	if (lastPath.empty()) {
		lastPath = File(midikraft::PatchDatabase::generateDefaultDatabaseLocation()).getParentDirectory().getFullPathName().toStdString();
	}

	File lastDirectory(lastPath);
	FileChooser databaseChooser("Please choose a directory with KnobKraft database files that will be exported...", lastDirectory);
	if (databaseChooser.browseForDirectory()) {
		Settings::instance().set("LastDatabaseMergePath", databaseChooser.getResult().getFullPathName().toStdString());
		// Find all databases
		Array<File> databases;
		databaseChooser.getResult().findChildFiles(databases, File::TypesOfFileToFind::findFiles, false, "*.db3");
		MergeAndExport mergeDialog(databases);
		mergeDialog.runThread();
	}
}

void OrmViews::mergeDatabases()
{
	std::string lastPath = Settings::instance().get("LastDatabaseMergePath", "");
	if (lastPath.empty()) {
		lastPath = File(midikraft::PatchDatabase::generateDefaultDatabaseLocation()).getParentDirectory().getFullPathName().toStdString();
	}

	File lastDirectory(lastPath);
	FileChooser databaseChooser("Please choose a directory with KnobKraft json files that will be imported and merged...", lastDirectory);
	if (databaseChooser.browseForDirectory()) {
		Settings::instance().set("LastDatabaseMergePath", databaseChooser.getResult().getFullPathName().toStdString());
		activePatchView().bulkImportPIP(databaseChooser.getResult());
	}
}

PopupMenu OrmViews::recentFileMenu() {
	PopupMenu menu;
	recentFiles_.createPopupMenuItems(menu, 3333, true, false);
	return menu;
}

void OrmViews::recentFileSelected(int selected)
{
	File databaseFile = recentFiles_.getFile(selected);
	if (databaseFile.existsAsFile()) {
		openDatabase(databaseFile);
	}
	else {
		AlertWindow::showMessageBox(AlertWindow::WarningIcon, "File not found", "That file no longer exists, cannot open!");
		recentFiles_.removeFile(databaseFile);
		persistRecentFileList();
	}
}

PopupMenu OrmViews::newViewMenu() {
	PopupMenu menu;
	int i = 4461;
	for (auto layout : getAvailableViews()) {
		menu.addItem(i++, layout, true, false);
	}
	return menu;
}

void OrmViews::newViewSelected(int selected) {
	auto l = getAvailableViews();
	if (selected >= 0 && selected < l.size()) {
		if (canCreateAdditionalViews(l[selected])) {
			dockManager_->openViewAsNewTab(l[selected], "", DropLocation::none);
		}
		else {
			dockManager_->showView(l[selected]);
		}
	}
}

PopupMenu OrmViews::loadLayoutMenu() {
	PopupMenu menu;
	int i = 4444;
	for (auto layout : layoutTemplates_) {
		menu.addItem(i++, layout.first, true, false);
	}
	return menu;
}

void OrmViews::layoutTemplateSelected(int selected) {
	if (selected >= 0 && selected < layoutTemplates_.size()) {
		auto template_s = layoutTemplates_[selected].second;
		MemoryInputStream ins(template_s.data(), template_s.size(), false);
		jassert(dockManager_);
		dockManager_->openLayout(ins);
	}
}

void OrmViews::saveLayoutTemplate() {
	MemoryOutputStream out;
	jassert(dockManager_);
	dockManager_->saveTemplate("Template", out);
	layoutTemplates_.push_back({ "Template", out.toString().toStdString() });
}


void OrmViews::persistRecentFileList()
{
	Settings::instance().set("RecentFiles", recentFiles_.toString().toStdString());
}

#ifndef _DEBUG
#ifdef USE_SENTRY
void OrmViews::checkUserConsent()
{
	auto userChoice = AlertWindow::showOkCancelBox(AlertWindow::QuestionIcon, "Asking for user consent",
		"This free software is developed in my spare time, which makes looking for potential problems a not so interesting part of this hobby.\n\n"
		"To shorten the time spent hunting for crashes, this software contains the capability to upload a minidump to the Internet for me to look at should the software crash (and only then).\n\n"
		"Press <Yes> to allow this helping me, or <No> to turn off crash reporting", "Yes", "No");
	if (!userChoice) {
		sentry_user_consent_revoke();
		Settings::instance().set("SentryConsent", "0");
		AlertWindow::showMessageBox(AlertWindow::InfoIcon, "No consent confirmation",
			"Thank you, I do understand and share your concern for privacy and information security.\n\nShould you change your mind, you find this box in the help menu!");
	}
	else {
		sentry_user_consent_give();
		Settings::instance().set("SentryConsent", "1");
		AlertWindow::showMessageBox(AlertWindow::InfoIcon, "Consent confirmation",
			"Thank you, I appreciate that!");
	}
}
#endif
#endif

void OrmViews::crashTheSoftware()
{
	if (AlertWindow::showOkCancelBox(AlertWindow::WarningIcon, "Test software crash", "This function is to check if the crash information upload works. When you press ok, the software will crash.\n\n"
		"Do you feel like it?")) {
		char* bad_idea = nullptr;
		(*bad_idea) = 0;
	}
}


std::unique_ptr<OrmViews> OrmViews::instance_;
