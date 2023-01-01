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
#include "MKS50.h"

#include "GenericAdaptation.h"
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

// Some command name constants
const std::string kRetrievePatches{ "retrieveActiveSynthPatches" };
const std::string kFetchEditBuffer{ "fetchEditBuffer" };
const std::string kReceiveManualDump{ "receiveManualDump" };
const std::string kLoadSysEx{ "loadsysEx" };
const std::string kExportSysEx{ "exportSysex" };
const std::string kExportPIF { "exportPIF" };
const std::string kShowDiff{ "showDiff" };
const std::string kSynthDetection{ "synthDetection" };
const std::string kLoopDetection{ "loopDetection" };
const std::string kSelectAdaptationDirect{ "selectAdaptationDir" };
const std::string kCreateNewAdaptation{ "createNewAdaptation" };

//
// Build a spdlog sink that goes into out standard log view
#include <spdlog/spdlog.h>
#include "SpdLogJuce.h"
#include <spdlog/sinks/base_sink.h>

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
MainComponent::MainComponent(bool makeYourOwnSize) :
	globalScaling_(1.0f),
	buttons_(301),
	mainTabs_(TabbedButtonBar::Orientation::TabsAtTop),
	logView_(true, true, true, true),
	midiLogArea_(&midiLogView_, BorderSize<int>(10)),
	logArea_(&logView_, BorderSize<int>(8))
{
	logger_ = std::make_unique<LogViewLogger>(logView_);

	// Setup spd logging
	std::vector<spdlog::sink_ptr> sinks;
	sinks.push_back(std::make_shared<LogViewSink_mt>(logView_));
	spdLogger_ = std::make_shared<spdlog::logger>("KnobKraftOrm", begin(sinks), end(sinks));
	spdLogger_->set_pattern("%H:%M:%S: %l %v");
	//spdLogger_->set_pattern("%Y-%m-%d %H:%M:%S.%e%z %l [%t] %v");
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
	recentFiles_.setMaxNumberOfItems(10);
	if (Settings::instance().keyIsSet("RecentFiles")) {
		recentFiles_.restoreFromString(Settings::instance().get("RecentFiles"));
	}

	automaticCategories_ = database_->getCategorizer();

	auto bcr2000 = std::make_shared <midikraft::BCR2000>();

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
	synths.emplace_back(midikraft::SynthHolder(bcr2000, buttonColour));

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

	refreshSynthList();

	autodetector_.addChangeListener(&synthList_);

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
		{5, { "View", { { "Scale 75%" }, { "Scale 100%" }, { "Scale 125%" }, { "Scale 150%" }, { "Scale 175%" }, { "Scale 200%" }}}},
		{6, { "Options", { { kCreateNewAdaptation}, { kSelectAdaptationDirect} }}},
		{6, { "Help", {
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
		setupView_->autoDetect();
	}, juce::KeyPress::F1Key } },
	{ "Import patches from synth", { kRetrievePatches, [this]() {
		patchView_->retrievePatches();
	}, juce::KeyPress::F7Key } },
	{ "Import edit buffer from synth",{ kFetchEditBuffer, [this]() {
		patchView_->retrieveEditBuffer();
	}, juce::KeyPress::F8Key  } },
	{ "Receive manual dump",{ kReceiveManualDump, [this]() {
		patchView_->receiveManualDump();
	}, juce::KeyPress::F9Key  } },
	{ "Import from files into database", { kLoadSysEx, [this]() {
		patchView_->loadPatches();
	}, juce::KeyPress::F3Key } },
	{ "Export into sysex files", { kExportSysEx , [this]() {
		patchView_->exportPatches();
	}}},
	{ "Export into PIF", { kExportPIF, [this]() {
		patchView_->createPatchInterchangeFile();
	} } },
	{ "Show patch comparison", { kShowDiff , [this]() {
		patchView_->showPatchDiffDialog();
	} } },
	{ "Quick check connectivity", { kSynthDetection, [this]() {
		setupView_->quickConfigure();
	}, juce::KeyPress::F2Key } },
	{ "Check for MIDI loops", { kLoopDetection, [this]() {
		setupView_->loopDetection();
	} } },
	{"Set User Adaptation Dir", { kSelectAdaptationDirect, [this]() {
		FileChooser directoryChooser("Please select the directory to store your user adaptations...", File(knobkraft::GenericAdaptation::getAdaptationDirectory()));
		if (directoryChooser.browseForDirectory()) {
			knobkraft::GenericAdaptation::setAdaptationDirectoy(directoryChooser.getResult().getFullPathName().toStdString());
			juce::AlertWindow::showMessageBox(AlertWindow::InfoIcon, "Restart required", "Your new adaptations directory will only be used after a restart of the application!");
		}
	} } },
	{"Create new adaptation", { kCreateNewAdaptation, [this]() {
		setupView_->createNewAdaptation();
	} } },

		//}, 0x44 /* D */, ModifierKeys::ctrlModifier } },
		{ "Edit categories", { "Edit categories", [this]() {
		EditCategoryDialog::showEditDialog(*database_, this, [this](std::vector<midikraft::CategoryDefinition> const& newDefinitions) {
			database_->updateCategories(newDefinitions);
			automaticCategories_ = database_->getCategorizer(); // Need to reload the automatic Categories!
			UIModel::instance()->categoriesChanged.sendChangeMessage();
		});
	} } },
		{ "Show category naming rules file", { "Show category naming rules file", [this]() {
		// This will create the file on demand, copying out the built-in information!
		if (!URL(automaticCategories_->getAutoCategoryFile().getFullPathName()).launchInDefaultBrowser()) {
			automaticCategories_->getAutoCategoryFile().revealToUser();
		}
	} } },	{ "Edit category import mapping", { "Edit category import mapping", [this]() {
		// This will create the file on demand, copying out the built-in information!
		if (!URL(automaticCategories_->getAutoCategoryMappingFile().getFullPathName()).launchInDefaultBrowser()) {
			automaticCategories_->getAutoCategoryMappingFile().revealToUser();
		}
	} } },
	{ "Rerun auto categorize...", { "Rerun auto categorize", [this]() {
		auto currentFilter = patchView_->currentFilter();
		int affected = database_->getPatchesCount(currentFilter);
		if (AlertWindow::showOkCancelBox(AlertWindow::QuestionIcon, "Re-run auto-categorization?",
			"Do you want to rerun the auto-categorization on the currently filtered " + String(affected) + " patches?\n\n"
			"This makes sense if you changed the auto category search strings, or the import mappings!\n\n"
			"And don't worry, if you have manually set categories (or manually removed categories that were auto-detected), this information is retained!"
			)) {
			automaticCategories_ = database_->getCategorizer(); // Need to reload the automatic Categories!
			AutoCategorizeWindow window(database_.get(), automaticCategories_, currentFilter, [this]() {
				patchView_->retrieveFirstPageFromDatabase();
			});
			window.runThread();
		}
	} } },
	{ "About", { "About", []() {
		aboutBox();
	}}},
	{ "Quit", { "Quit", []() {
		JUCEApplicationBase::quit();
	}}},
		//, 0x51 /* Q */, ModifierKeys::ctrlModifier}}
		{ "Scale 75%", { "Scale 75%", [this]() { setZoomFactor(0.75f); }}},
		{ "Scale 100%", { "Scale 100%", [this]() { setZoomFactor(1.0f); }}},
		{ "Scale 125%", { "Scale 125%", [this]() { setZoomFactor(1.25f); }}},
		{ "Scale 150%", { "Scale 150%", [this]() { setZoomFactor(1.5f); }}},
		{ "Scale 175%", { "Scale 175%", [this]() { setZoomFactor(1.75f); }}},
		{ "Scale 200%", { "Scale 200%", [this]() { setZoomFactor(2.0f); }}},
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
			patchView_->bulkRenamePatches();
		}}},
		{ "Delete patches...", { "Delete patches...", [this] {
			patchView_->deletePatches();
		}}},
		{ "Reindex patches...", { "Reindex patches...", [this] {
			patchView_->reindexPatches();
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
	getTopLevelComponent()->addKeyListener(commandManager_.getKeyMappings());

	// Setup menu structure
	menuModel_ = std::make_unique<LambdaMenuModel>(menuStructure, &commandManager_, &buttons_);
	menuModel_->setApplicationCommandManagerToWatch(&commandManager_);
	menuBar_.setModel(menuModel_.get());
	addAndMakeVisible(menuBar_);

	// Create the patch view
	patchView_ = std::make_unique<PatchView>(*database_, synths);
	//patchView_ = std::make_unique<PatchView>(commandManager_, *database_, synths, automaticCategories_);
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
		case KeyboardMacroEvent::Unknown:
			// Fall through
		default:
			spdlog::error("Invalid keyboard macro event detected");
			return;
		}
		spdlog::debug("Keyboard Macro event fired {}", KeyboardMacro::toText(event));
		});

	// Create the BCR2000 view, the predecessor to the generic editor view
	//bcr2000View_ = std::make_unique<BCR2000_Component>(bcr2000);

	addAndMakeVisible(synthList_);
	addAndMakeVisible(patchList_);
	Colour tabColour = getUIColour(LookAndFeel_V4::ColourScheme::UIColour::widgetBackground);
	mainTabs_.addTab("Library", tabColour, patchView_.get(), false);
	//mainTabs_.addTab("Editor", tabColour, bcr2000View_.get(), false);
	//mainTabs_.addTab("Audio In", tabColour, recordingView_.get(), false);
	mainTabs_.addTab("Settings", tabColour, settingsView_.get(), false);
	mainTabs_.addTab("Macros", tabColour, keyboardView_.get(), false);
	mainTabs_.addTab("Setup", tabColour, setupView_.get(), false);
	mainTabs_.addTab("MIDI Log", tabColour, &midiLogArea_, false);

	addAndMakeVisible(menuBar_);
	splitter_ = std::make_unique<SplitteredComponent>("LogSplitter", SplitteredEntry{ &mainTabs_, 80, 20, 100 }, SplitteredEntry{ &logArea_, 20, 5, 50 }, false);
	addAndMakeVisible(splitter_.get());

	UIModel::instance()->currentSynth_.addChangeListener(&synthList_);
	UIModel::instance()->currentSynth_.addChangeListener(this);
	UIModel::instance()->synthList_.addChangeListener(this);

	// Is the active Synth persisted and active?
	std::string activeSynthName = Settings::instance().get("CurrentSynth", "");
	auto persistedSynth = UIModel::instance()->synthList_.synthByName(activeSynthName);
	if (persistedSynth.device() && UIModel::instance()->synthList_.isSynthActive(persistedSynth.device())) {
		UIModel::instance()->currentSynth_.changeCurrentSynth(persistedSynth.synth());
		synthList_.setActiveListItem(activeSynthName);
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

	// Install our MidiLogger
	midikraft::MidiController::instance()->setMidiLogFunction([this](const MidiMessage& message, const String& source, bool isOut) {
		midiLogView_.addMessageToList(message, source, isOut);
		});

	// Do a quickconfigure
	auto list = UIModel::instance()->synthList_.activeSynths();
	autodetector_.quickconfigure(list);
	// Refresh Setup View with the result of this
	UIModel::instance()->currentSynth_.sendChangeMessage();

	// Monitor the list of available MIDI devices
	midikraft::MidiController::instance()->addChangeListener(this);

	// If there is no synth configured, like, on first launch, show the Setup tab instead of the default Library tab
	if (list.empty()) {
		int setupIndex = findIndexOfTabWithNameEnding(&mainTabs_, "Setup");
		mainTabs_.setCurrentTabIndex(setupIndex, false);
	}

	// Feel free to request the globals page from the active synth
	settingsView_->loadGlobals();

	// Make sure you set the size of the component after
	// you add any child components.
	if (makeYourOwnSize) {
		juce::Rectangle<int> mainScreenSize = Desktop::getInstance().getDisplays().getPrimaryDisplay()->userArea;
		auto initialSize = mainScreenSize.reduced(100);
		setSize(initialSize.getWidth(), initialSize.getHeight());
	}

	// Refresh Window title and other things to do when the MainComponent is displayed
	MessageManager::callAsync([this]() {
		UIModel::instance()->windowTitle_.sendChangeMessage();
#ifdef WIN32
		checkForUpdatesOnStartup();
#endif
		});

#ifndef _DEBUG
#ifdef USE_SENTRY
	auto consentAlreadyGiven = Settings::instance().get("SentryConsent", "unknown");
	if (consentAlreadyGiven == "unknown") {
		checkUserConsent();
	}
	else {
		if (consentAlreadyGiven == "0") {
			sentry_user_consent_revoke();
		}
		else if (consentAlreadyGiven == "1") {
			sentry_user_consent_give();
		}
	}
#endif
#endif
}

MainComponent::~MainComponent()
{
	// Prevent memory leaks being reported on shutdown
	EditCategoryDialog::shutdown();
	ExportDialog::shutdown();

#ifdef USE_SPARKLE
#ifdef WIN32
	win_sparkle_cleanup();
#endif
#endif
	UIModel::instance()->synthList_.removeChangeListener(this);
	UIModel::instance()->currentSynth_.removeChangeListener(&synthList_);
	UIModel::instance()->currentSynth_.removeChangeListener(this);

	Logger::setCurrentLogger(nullptr);
}

#ifdef USE_SPARKLE
void logSparkleError() {
	spdlog::error("Error encountered in WinSparkle");
}

void sparkleInducedShutdown() {
	MessageManager::callAsync([]() {
		JUCEApplicationBase::quit();
		});
}
#endif

void MainComponent::checkForUpdatesOnStartup() {
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

void MainComponent::createNewDatabase()
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
		recentFiles_.addFile(File(database_->getCurrentDatabaseFileName()));
		if (database_->switchDatabaseFile(databaseFile.getFullPathName().toStdString(), midikraft::PatchDatabase::OpenMode::READ_WRITE)) {
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

void MainComponent::openDatabase()
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

void MainComponent::openDatabase(File& databaseFile)
{
	if (databaseFile.existsAsFile()) {
		recentFiles_.addFile(File(database_->getCurrentDatabaseFileName()));
		if (database_->switchDatabaseFile(databaseFile.getFullPathName().toStdString(), midikraft::PatchDatabase::OpenMode::READ_WRITE)) {
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

void MainComponent::saveDatabaseAs()
{
	std::string lastPath = Settings::instance().get("LastDatabasePath", "");
	if (lastPath.empty()) {
		lastPath = File(midikraft::PatchDatabase::generateDefaultDatabaseLocation()).getParentDirectory().getFullPathName().toStdString();
	}
	File lastDirectory(lastPath);
	FileChooser databaseChooser("Please choose a new KnobKraft Orm SQlite database file...", lastDirectory, "*.db3");
	if (databaseChooser.browseForFileToSave(true)) {
		File databaseFile = databaseChooser.getResult();
		database_->makeDatabaseBackup(databaseFile);
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

void MainComponent::exportDatabases()
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

void MainComponent::mergeDatabases()
{
	std::string lastPath = Settings::instance().get("LastDatabaseMergePath", "");
	if (lastPath.empty()) {
		lastPath = File(midikraft::PatchDatabase::generateDefaultDatabaseLocation()).getParentDirectory().getFullPathName().toStdString();
	}

	File lastDirectory(lastPath);
	FileChooser databaseChooser("Please choose a directory with KnobKraft json files that will be imported and merged...", lastDirectory);
	if (databaseChooser.browseForDirectory()) {
		Settings::instance().set("LastDatabaseMergePath", databaseChooser.getResult().getFullPathName().toStdString());
		patchView_->bulkImportPIP(databaseChooser.getResult());
	}
}

PopupMenu MainComponent::recentFileMenu() {
	PopupMenu menu;
	recentFiles_.createPopupMenuItems(menu, 3333, true, false);
	return menu;
}

void MainComponent::recentFileSelected(int selected)
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

void MainComponent::persistRecentFileList()
{
	Settings::instance().set("RecentFiles", recentFiles_.toString().toStdString());
}

#ifndef _DEBUG
#ifdef USE_SENTRY
void MainComponent::checkUserConsent()
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

void MainComponent::crashTheSoftware()
{
	if (AlertWindow::showOkCancelBox(AlertWindow::WarningIcon, "Test software crash", "This function is to check if the crash information upload works. When you press ok, the software will crash.\n\n"
		"Do you feel like it?")) {
		char* bad_idea = nullptr;
		(*bad_idea) = 0;
	}
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

void MainComponent::resized()
{
	auto area = getLocalBounds();
	menuBar_.setBounds(area.removeFromTop(LookAndFeel::getDefaultLookAndFeel().getDefaultMenuBarHeight()));
	//auto topRow = area.removeFromTop(40).withTrimmedLeft(8).withTrimmedRight(8).withTrimmedTop(8);
	//patchList_.setBounds(topRow);

	if (UIModel::instance()->synthList_.activeSynths().size() > 1) {
		auto secondTopRow = area.removeFromTop(LAYOUT_LINE_SPACING + 20 + LAYOUT_INSET_NORMAL)
			.withTrimmedLeft(LAYOUT_INSET_NORMAL).withTrimmedRight(LAYOUT_INSET_NORMAL).withTrimmedTop(LAYOUT_INSET_NORMAL);
		synthList_.setBounds(secondTopRow);
		synthList_.setVisible(true);
	}
	else {
		//TODO - one synth needs to be implemented differently.
// At most one synth selected - do not display the large synth selector row you need when you use the software with multiple synths
		synthList_.setVisible(false);
	}
	splitter_->setBounds(area);
}

void MainComponent::shutdown()
{
	// Shutdown database, which will make a backup
	database_.reset();
}

std::string MainComponent::getDatabaseFileName() const
{
	return database_->getCurrentDatabaseFileName();
}

void MainComponent::refreshSynthList() {
	std::vector<std::shared_ptr<ActiveListItem>> listItems;
	std::vector<midikraft::PatchHolder> patchList;
	auto currentPatches = UIModel::instance()->currentPatch_.allCurrentPatches();
	for (auto s : UIModel::instance()->synthList_.allSynths()) {
		if (UIModel::instance()->synthList_.isSynthActive(s.device())) {
			listItems.push_back(std::make_shared<ActiveSynthHolder>(s.device(), s.color()));
			if (currentPatches.find(s.device()->getName()) != currentPatches.end()) {
				patchList.push_back(currentPatches[s.device()->getName()]);
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

	synthList_.setList(listItems, [](std::shared_ptr<ActiveListItem> const& clicked) {
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
	patchList_.setPatches(patchList);
}

void MainComponent::changeListenerCallback(ChangeBroadcaster* source)
{
	if (source == midikraft::MidiController::instance()) {
		// Kick off a new quickconfigure, as the MIDI interface setup has changed and synth available will be different
		auto synthList = UIModel::instance()->synthList_.activeSynths();
		quickconfigreDebounce_.callDebounced([this, synthList]() {
			auto myList = synthList;
			autodetector_.quickconfigure(myList);
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
			synthList_.setActiveListItem(synth->getName());
		}


		// The active synth has been switched, make sure to refresh the tab name properly
		int index = findIndexOfTabWithNameEnding(&mainTabs_, "settings");
		if (index != -1) {
			// Rename tab to show settings of this synth
			if (UIModel::currentSynth()) {
				mainTabs_.setTabName(index, UIModel::currentSynth()->getName() + " settings");
			}
		}
		else {
			mainTabs_.setTabName(index, "Settings");
		}


		// The active synth has been switched, check if it is an adaptation and then refresh the adaptation view
		auto adaptation = std::dynamic_pointer_cast<knobkraft::GenericAdaptation>(UIModel::instance()->currentSynth_.smartSynth());
		Colour tabColour = getUIColour(LookAndFeel_V4::ColourScheme::UIColour::widgetBackground);
		if (adaptation) {
			adaptationView_.setupForAdaptation(adaptation);
			int i = findIndexOfTabWithNameEnding(&mainTabs_, "Adaptation");
			if (i == -1) {
				// Need to add the tab back in
				mainTabs_.addTab("Adaptation", tabColour, &adaptationView_, false, 2);
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
						mainTabs_.addTab(UIModel::currentSynth()->getName() + " settings", tabColour, settingsView_.get(), false, 1);
					}
					else {
						mainTabs_.addTab("Settings", tabColour, settingsView_.get(), false, 1);
					}
				}
				i = findIndexOfTabWithNameEnding(&mainTabs_, "Adaptation");
				if (mainTabs_.getCurrentTabIndex() == i) {
					mainTabs_.setCurrentTabIndex(i - 1);
				}
				mainTabs_.removeTab(i);
			}
		}
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

