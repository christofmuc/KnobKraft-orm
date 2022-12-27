#pragma once

#include "AutoDetection.h"
#include "Librarian.h"

#include "PatchDatabase.h"
#include "AutomaticCategory.h"
#include "LambdaButtonStrip.h"
#include "LambdaMenuModel.h"

#include "OrmLookAndFeel.h"

#include <spdlog/logger.h>

#pragma warning( push )
#pragma warning( disable : 4100 )
#include <docks/docks.h>
#pragma warning( pop )

class LogView;
class MidiLogView;
class SetupView;
class PatchView;

class OrmDockableWindow : public Component
{
public:
	OrmDockableWindow();
	virtual ~OrmDockableWindow() override;

private:
	OrmLookAndFeel ormLookAndFeel_;
};

class OrmViews : public DockManager::Delegate
{
public:
	OrmViews();
	virtual ~OrmViews() override;
	static OrmViews& instance();
	static void shutdown();

	// Initializer
	void init(DockManager* dockManager);

	// Access to the single Setup View
	std::shared_ptr<SetupView> setupView() { return setupView_;  }

	// Some global logic objects that we need only once
	midikraft::Librarian& librarian();
	midikraft::AutoDetection& autoDetector();
	midikraft::PatchDatabase& patchDatabase();
	std::shared_ptr<midikraft::AutomaticCategory> automaticCategories();

	// A little bit of extra state - this needs refactoring
	void reloadAutomaticCategories();

	// Implementation of DockManager::Delegate
	virtual const juce::StringArray getAvailableViews() const override;
	virtual std::shared_ptr<juce::Component> createView(const juce::String& nameOfViewToCreate) override;
	virtual const juce::String getDefaultWindowName() const override;
	virtual std::shared_ptr<DockingWindow> createTopLevelWindow(DockManager& manager, DockManagerData& data, const juce::ValueTree& tree) override;

	std::shared_ptr<juce::MenuBarModel> getMainMenu();
	
	PatchView& activePatchView();

private:
	Component* activeMainWindow();

	bool canCreateAdditionalViews(juce::String const& name);

	void checkForUpdatesOnStartup();
	void createNewDatabase();
	void openDatabase();
	void openDatabase(File& databaseFile);
	void saveDatabaseAs();
	static void exportDatabases();
	void mergeDatabases();
	PopupMenu newViewMenu();
	void newViewSelected(int selected);
	PopupMenu recentFileMenu();
	void recentFileSelected(int selected);
	PopupMenu loadLayoutMenu();
	void layoutTemplateSelected(int selected);
	void saveLayoutTemplate();
	void persistRecentFileList();
#ifndef _DEBUG
#ifdef USE_SENTRY
	void checkUserConsent();
#endif
#endif
	static void crashTheSoftware();

	midikraft::Librarian librarian_;
	midikraft::AutoDetection autodetector_;
	std::unique_ptr<midikraft::PatchDatabase> database_;
	std::shared_ptr<midikraft::AutomaticCategory> automaticCategories_;

	// The infrastructure for the menu and the short cut keys
	RecentlyOpenedFilesList recentFiles_;
	std::shared_ptr<LambdaMenuModel> menuModel_;
	LambdaButtonStrip buttons_;
	ApplicationCommandManager commandManager_;

	std::vector<std::pair<std::string, std::string>> layoutTemplates_;

	// These Views are create only once and then reused.
	std::shared_ptr<SetupView> setupView_;
	std::shared_ptr<LogView> logView_;
	std::shared_ptr<MidiLogView> midiLogView_;

	// These views can be created more than once
	std::vector<std::shared_ptr<PatchView>> patchViews_;

	// Global logger object
	std::shared_ptr<spdlog::logger> spdLogger_;

	// Reference to DockManager
	DockManager* dockManager_;

	static std::unique_ptr<OrmViews> instance_;
};
