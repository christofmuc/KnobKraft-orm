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
#include "SplitteredComponent.h"
#include "CategoryButtons.h"
#include "CurrentPatchDisplay.h"
#include "CollapsibleContainer.h"
#include "PatchListTree.h"
#include "RecycleBin.h"

#include "PatchDatabase.h"
#include "PatchHolder.h"
#include "AutomaticCategory.h"

#include "ImportFromSynthDialog.h"

#include <map>

class PatchDiff;
class PatchSearchComponent;

class PatchView : public Component,
	public DragAndDropContainer,
	private ChangeListener
{
public:
	PatchView(midikraft::PatchDatabase &database, std::vector<midikraft::SynthHolder> const &synths);
	virtual ~PatchView();

	void resized() override;

	// React on synth or patch changed
	virtual void changeListenerCallback(ChangeBroadcaster* source) override;

	void retrieveFirstPageFromDatabase();

	// Macro controls triggered by the MidiKeyboard
	void hideCurrentPatch();
	void favoriteCurrentPatch();
	void selectPreviousPatch();
	void selectNextPatch();
	void retrieveEditBuffer();

	// Protected functions that are potentially dangerous and are only called via the main menu
	void deletePatches();
	void reindexPatches();

	// Additional functions for the auto thumbnailer
	int totalNumberOfPatches();
	void selectFirstPatch();

	// Hand through from PatchSearch
	midikraft::PatchFilter currentFilter();

	// Special functions
	void bulkImportPIP(File directory);

private:
	friend class PatchSearchComponent;

	std::vector<CategoryButtons::Category> predefinedCategories();

	void loadPage(int skip, int limit, std::function<void(std::vector<midikraft::PatchHolder>)> callback);

	void retrievePatches();

	std::vector<midikraft::PatchHolder> autoCategorize(std::vector<midikraft::PatchHolder> const &patches);

	void loadPatches();	
	void receiveManualDump();
	void exportPatches();
	void updateLastPath();
	void createPatchInterchangeFile();
	void mergeNewPatches(std::vector<midikraft::PatchHolder> patchesLoaded);
	void selectPatch(midikraft::PatchHolder &patch, bool alsoSendToSynth);
	void showPatchDiffDialog();
	void saveCurrentPatchCategories();
	void setImportListFilter(String filter);
	void setUserListFilter(String filter);
	void deleteSomething(nlohmann::json const &infos);

	PatchListTree patchListTree_;
	std::string sourceFilterID_; // This is the old "import" combo box in new
	std::string listFilterID_;
	std::unique_ptr<SplitteredComponent> splitters_;

	RecycleBin recycleBin_;

	Label patchLabel_;
	LambdaButtonStrip buttonStrip_;
	std::unique_ptr<PatchSearchComponent> patchSearch_;
	std::unique_ptr<PatchButtonPanel> patchButtons_;
	std::unique_ptr<CurrentPatchDisplay> currentPatchDisplay_;
	std::unique_ptr<ImportFromSynthDialog> importDialog_;
	std::unique_ptr<PatchDiff> diffDialog_;

	midikraft::Librarian librarian_;

	std::vector<midikraft::SynthHolder> synths_;
	int currentLayer_;

	midikraft::PatchHolder compareTarget_;

	midikraft::PatchDatabase &database_;
	
	std::string lastPathForPIF_;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PatchView)
};

