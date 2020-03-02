/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "JuceHeader.h"

#include "LambdaButtonStrip.h"
#include "Librarian.h"
#include "MidiController.h"
#include "Logger.h"

#include "Patch.h"
#include "Synth.h"
#include "SynthHolder.h"

#include "PatchButtonPanel.h"
#include "CategoryButtons.h"
#include "CurrentPatchDisplay.h"

#include "PatchDatabase.h"
#include "PatchHolder.h"

#include "ImportFromSynthDialog.h"

#include <map>

class PatchDiff;

class PatchView : public Component,
	private ComboBox::Listener,
	private ToggleButton::Listener,
	private ChangeListener
{
public:
	PatchView(midikraft::PatchDatabase &database, std::vector<midikraft::SynthHolder> const &synths);
	virtual ~PatchView();

	void resized() override;

	// ComboBox Listener
	void comboBoxChanged(ComboBox* box) override;

	// Button listener
	void buttonClicked(Button* button) override;

	// React on synth or patch changed
	virtual void changeListenerCallback(ChangeBroadcaster* source) override;

	midikraft::PatchDatabase::PatchFilter buildFilter();
	void retrieveFirstPageFromDatabase();

private:
	static std::vector<CategoryButtons::Category> predefinedCategories();
	
	
	void loadPage(int skip, int limit, std::function<void(std::vector<midikraft::PatchHolder>)> callback);

	void retrievePatches();
	void retrieveEditBuffer();

	void loadPatches();
	std::string currentlySelectedSourceUUID();
	void rebuildImportFilterBox();
	void mergeNewPatches(std::vector<midikraft::PatchHolder> patchesLoaded);
	void selectPatch(midikraft::Synth &synth, midikraft::PatchHolder &patch);
	void showPatchDiffDialog();
	void saveCurrentPatchCategories();

	ComboBox importList_;
	CategoryButtons categoryFilters_;
	ToggleButton onlyFaves_;
	ToggleButton showHidden_;
	ToggleButton onlyUntagged_;
	Label patchLabel_;
	LambdaButtonStrip buttonStrip_;
	std::unique_ptr<PatchButtonPanel> patchButtons_;
	std::unique_ptr<CurrentPatchDisplay> currentPatchDisplay_;
	std::unique_ptr<ImportFromSynthDialog> importDialog_;
	std::unique_ptr<PatchDiff> diffDialog_;

	midikraft::Librarian librarian_;

	std::vector<midikraft::SynthHolder> synths_;
	std::vector<midikraft::ImportInfo> imports_;
	int currentLayer_;

	midikraft::PatchHolder compareTarget_;

	midikraft::PatchDatabase &database_;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PatchView)
};

