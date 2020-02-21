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
#include "AutoDetection.h"
#include "PropertyEditor.h"

#include "PatchView.h"
#include "SettingsView.h"
#include "Rev2.h"

class LogViewLogger;

class MainComponent : public Component
{
public:
    MainComponent();
    ~MainComponent();

    void resized() override;

	void refreshListOfPresets();

private:
	void detectBCR();
	void refreshFromBCR();
	void retrievePatch(int no);

	void aboutBox();

	midikraft::AutoDetection autodetector_;
	std::shared_ptr<midikraft::Rev2> rev2_;
	TabbedComponent mainTabs_;
	LogView logView_;
	std::unique_ptr<PatchView> patchView_;
	StretchableLayoutManager stretchableManager_;
	StretchableLayoutResizerBar resizerBar_;
	MidiLogView midiLogView_;
	InsetBox midiLogArea_;
	std::unique_ptr<SettingsView> settingsView_;
	std::unique_ptr<LogViewLogger> logger_;
	std::vector<MidiMessage> currentDownload_;
	MenuBarComponent menuBar_;

	InsetBox logArea_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
