/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

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
#include "OrmLookAndFeel.h"

#include "OrmViews.h"

class LogViewLogger;

// Each top level Window that takes docked views is one of these
class MainComponent : public DockingWindow, public DragAndDropContainer, private ChangeListener
{
public:
	MainComponent(DockManager& manager, DockManagerData& data, const juce::ValueTree& tree);
    virtual ~MainComponent() override;

	std::string getDatabaseFileName() const; // This is only there to expose it to the MainApplication for the Window Title?

	virtual void closeButtonPressed() override;

	static void aboutBox();
private:

	void setZoomFactor(float newZoomInPercentage) const;
    float calcAcceptableGlobalScaleFactor();
	Colour getUIColour(LookAndFeel_V4::ColourScheme::UIColour colourToGet);
	void refreshSynthList();

	virtual void changeListenerCallback(ChangeBroadcaster* source) override;
	
	// Helper function because of JUCE API
	static int findIndexOfTabWithNameEnding(TabbedComponent *mainTabs, String const &name);

	// For display size support. This will be filled before we modify any global scales
	float globalScaling_;

	// For kicking off new quickconfigures automatically
	DebounceTimer quickconfigreDebounce_;

	std::shared_ptr<juce::MenuBarModel> menu_;
	MenuBarComponent menuBar_;

	OrmLookAndFeel ormLookAndFeel_;	
	
	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
