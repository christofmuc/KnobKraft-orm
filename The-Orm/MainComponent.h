/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "JuceHeader.h"

#include "LogView.h"
#include "MidiLogView.h"
#include "PatchButtonGrid.h"
#include "InsetBox.h"
#include "DebounceTimer.h"
#include "SplitteredComponent.h"

#include "PatchDatabase.h"
#include "AutoDetection.h"
#include "AutomaticCategory.h"
#include "PropertyEditor.h"
#include "SynthList.h"
#include "LambdaMenuModel.h"
#include "LambdaButtonStrip.h"
#include "PatchPerSynthList.h"
#include "PatchListTree.h"

#include "PatchView.h"
#include "SettingsView.h"
#include "KeyboardMacroView.h"
#include "SetupView.h"
#include "RecordingView.h"
#include "BCR2000_Component.h"
#include "AdaptationView.h"

#include "OrmViews.h"

class LogViewLogger;

class MainComponent : public DockingWindow, private ChangeListener
{
public:
	MainComponent(DockManager& manager, DockManagerData& data, const juce::ValueTree& tree);
    virtual ~MainComponent() override;

	//virtual void resized() override;

	void shutdown();

	std::string getDatabaseFileName() const; // This is only there to expose it to the MainApplication for the Window Title?

	virtual void closeButtonPressed() override;

private:
	void checkForUpdatesOnStartup();
	void createNewDatabase();
	void openDatabase();
	void openDatabase(File &databaseFile);
	void saveDatabaseAs();
	static void exportDatabases();
	void mergeDatabases();
	PopupMenu recentFileMenu();
	void recentFileSelected(int selected);
	void persistRecentFileList();
#ifndef _DEBUG
#ifdef USE_SENTRY
	void checkUserConsent();
#endif
#endif
	static void crashTheSoftware();

	void setZoomFactor(float newZoomInPercentage) const;
    float calcAcceptableGlobalScaleFactor();
	Colour getUIColour(LookAndFeel_V4::ColourScheme::UIColour colourToGet);
	void refreshSynthList();
	static void aboutBox();

	virtual void changeListenerCallback(ChangeBroadcaster* source) override;
	
	// Helper function because of JUCE API
	static int findIndexOfTabWithNameEnding(TabbedComponent *mainTabs, String const &name);

	RecentlyOpenedFilesList recentFiles_;

	// For display size support. This will be filled before we modify any global scales
	float globalScaling_;

	// For kicking off new quickconfigures automatically
	DebounceTimer quickconfigreDebounce_;

	// The infrastructure for the menu and the short cut keys
	std::unique_ptr<LambdaMenuModel> menuModel_;
	LambdaButtonStrip buttons_;
	ApplicationCommandManager commandManager_;
	MenuBarComponent menuBar_;

	SynthList synthList_;
	PatchPerSynthList patchList_;
	std::unique_ptr<PatchView> patchView_;
	std::unique_ptr<SplitteredComponent> splitter_;
	std::unique_ptr<RecordingView> recordingView_;
	std::unique_ptr<BCR2000_Component> bcr2000View_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
