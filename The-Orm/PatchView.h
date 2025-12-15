/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "JuceHeader.h"

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
#include "SynthBankPanel.h"
#include "PatchHistoryPanel.h"

#include <map>

class PatchDiff;
class PatchSearchComponent;

class PatchView : public Component,
	public DragAndDropContainer,
	private ChangeListener
{
public:
	PatchView(midikraft::PatchDatabase &database, std::vector<midikraft::SynthHolder> const &synths);
	virtual ~PatchView() override;

	void resized() override;

	// React on synth or patch changed
	virtual void changeListenerCallback(ChangeBroadcaster* source) override;

	void retrieveFirstPageFromDatabase();
	std::shared_ptr<midikraft::PatchList> retrieveListFromDatabase(midikraft::ListInfo const& info);
	void loadSynthBankFromDatabase(std::shared_ptr<midikraft::Synth> synth, MidiBankNumber bank, std::string const& bankId);
	void retrieveBankFromSynth(std::shared_ptr<midikraft::Synth> synth, MidiBankNumber bank, std::function<void()> finishedHandler);
	void sendBankToSynth(std::shared_ptr<midikraft::SynthBank> bankToSend, bool ignoreDirty, std::function<void()> finishedHandler);
	void selectPatch(midikraft::PatchHolder& patch, bool alsoSendToSynth);

	// Macro controls triggered by the MidiKeyboard
	void hideCurrentPatch();
	void favoriteCurrentPatch();
	void regularCurrentPatch();
	void selectPreviousPatch();
	void selectNextPatch();
	void retrieveEditBuffer();

	// Protected functions that are potentially dangerous and are only called via the main menu
	void retrievePatches();
	void bulkRenamePatches();
	void receiveManualDump();
	void deletePatches();
	void refreshAllAfterDelete();
	void reindexPatches();
	void loadPatches();
	void exportPatches();
	void exportBank();
	void createPatchInterchangeFile();
	void showPatchDiffDialog();

	// Additional functions for the auto thumbnailer
	int totalNumberOfPatches();
	void selectFirstPatch();

	// Hand through from PatchSearch
	midikraft::PatchFilter currentFilter();

	// Special functions
	void bulkImportPIP(File directory);

	// New for bank management
	midikraft::PatchFilter bankFilter(std::shared_ptr<midikraft::Synth> synth, std::string const& listID);
    void copyBankPatchNamesToClipboard();

private:
	friend class PatchSearchComponent;
	friend class SimplePatchGrid;

	std::vector<CategoryButtons::Category> predefinedCategories();

	int getTotalCount();
	void loadPage(int skip, int limit, midikraft::PatchFilter const& filter, std::function<void(std::vector<midikraft::PatchHolder>)> callback);

	std::vector<midikraft::PatchHolder> autoCategorize(std::vector<midikraft::PatchHolder> const &patches);

	// TODO These should go into a more general library
	std::vector<MidiProgramNumber> patchIsInSynth(midikraft::PatchHolder& patch);
	static bool isSynthConnected(std::shared_ptr<midikraft::Synth> synth);
	static std::vector<MidiMessage> buildSelectBankAndProgramMessages(MidiProgramNumber program, midikraft::PatchHolder& patch);

	// Helper functions
	void sendProgramChangeMessagesForPatch(std::shared_ptr<midikraft::MidiLocationCapability> midiLocation, MidiProgramNumber program, midikraft::PatchHolder& patch);
	static void sendPatchAsSysex(midikraft::PatchHolder& patch);

	void updateLastPath();

	void mergeNewPatches(std::vector<midikraft::PatchHolder> patchesLoaded);
	void downloadBanksFromSynth(std::shared_ptr<midikraft::Synth> synth,
		const std::vector<MidiBankNumber>& banks,
		const juce::String& progressTitle,
		std::function<void(std::vector<midikraft::PatchHolder>)> onLoaded,
		bool requireDetectedDevice = true);
	
	void saveCurrentPatchCategories();
	void setSynthBankFilter(std::shared_ptr<midikraft::Synth> synth, MidiBankNumber bank);
	void setUserBankFilter(std::shared_ptr<midikraft::Synth> synth, std::string const& listId);
	void setListFilter(String filter, std::shared_ptr<midikraft::Synth> synth = nullptr);
	void deleteSomething(nlohmann::json const &infos);

	void fillList(std::shared_ptr<midikraft::PatchList> list, CreateListDialog::TFillParameters fillParameters, std::function<void()> finishedCallback);

	void showBank();

    PatchListTree patchListTree_;
	std::string listFilterID_;
	std::shared_ptr<midikraft::Synth> listFilterSynth_;
	std::unique_ptr<SplitteredComponent> splitters_;
	TabbedComponent rightSideTab_;

	RecycleBin recycleBin_;

	Label patchLabel_;
	std::unique_ptr<PatchSearchComponent> patchSearch_;
	std::unique_ptr<PatchButtonPanel> patchButtons_;
	std::unique_ptr<CurrentPatchDisplay> currentPatchDisplay_;
	std::unique_ptr<SynthBankPanel> synthBank_;
	std::unique_ptr<PatchHistoryPanel> patchHistory_;
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
