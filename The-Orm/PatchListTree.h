/*
   Copyright (c) 2021 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "JuceHeader.h"

#include "PatchDatabase.h"
#include "SynthHolder.h"
#include "TreeViewNode.h"
#include "CreateListDialog.h"


class PatchListTree : public Component, private ChangeListener {
public:
	typedef std::function<void(String)> TSelectionHandler;
	typedef std::function<void(std::shared_ptr<midikraft::Synth>, MidiBankNumber)> TBankSelectionHandler;
	typedef std::function<void(std::shared_ptr<midikraft::Synth>, String)> TUserBankSelectionHandler;
	typedef std::function<void(midikraft::PatchHolder)> TPatchSelectionHandler;
	typedef std::function<void(std::shared_ptr<midikraft::PatchList>, CreateListDialog::TFillParameters, std::function<void()>)> TPatchListFillHandler;


	PatchListTree(midikraft::PatchDatabase &db, std::vector<midikraft::SynthHolder> const& synths);
	virtual ~PatchListTree() override;

	TSelectionHandler onImportListSelected;
	TBankSelectionHandler onSynthBankSelected;
	TUserBankSelectionHandler onUserBankSelected;
	TSelectionHandler onUserListSelected;
	TSelectionHandler onUserListChanged;

	TPatchSelectionHandler onPatchSelected;
	TPatchListFillHandler onPatchListFill;

	virtual void resized() override;

	void refreshAllUserLists(std::function<void()> onFinished);
	void refreshAllImports(std::function<void()> onFinished);
	void refreshChildrenOfListId(std::string const& list_id, std::function<void()> onFinished);
	void refreshParentOfListId(std::string const& list_id, std::function<void()> onFinished);

	void selectAllIfNothingIsSelected();
	void selectItemByPath(std::vector<std::string> const& path);

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PatchListTree)
	
private:
	void regenerateUserLists(std::function<void()> onFinished);
	void regenerateImportLists(std::function<void()> onFinished);

	void selectSynthLibrary(std::string const& synthName);
	std::string getSelectedSynth() const;
	bool isUserListSelected() const;
	std::list<std::string> pathOfSelectedItem() const;
	TreeViewNode* findNodeForListID(std::string const& list_id);

	TreeViewNode* newTreeViewItemForPatch(midikraft::ListInfo list, midikraft::PatchHolder patchHolder, int index);
	TreeViewNode* newTreeViewItemForSynthBanks(std::shared_ptr<midikraft::SimpleDiscoverableDevice> synth);
	TreeViewNode* newTreeViewItemForStoredBanks(std::shared_ptr<midikraft::SimpleDiscoverableDevice> synth);
	TreeViewNode* newTreeViewItemForImports(std::shared_ptr<midikraft::SimpleDiscoverableDevice> synth);
	TreeViewNode* newTreeViewItemForUserBank(std::shared_ptr<midikraft::Synth> synth, TreeViewNode* parent, midikraft::ListInfo list);
	TreeViewNode* newTreeViewItemForPatchList(midikraft::ListInfo list);

	void changeListenerCallback(ChangeBroadcaster* source) override;

	std::map<std::string, std::weak_ptr<midikraft::Synth>> synths_; // The database needs this to load patch lists
	//std::map<std::string, std::unique_ptr<XmlElement>> synthSpecificTreeState_;

	midikraft::PatchDatabase& db_;

	std::unique_ptr<TreeView> treeView_;
	TreeViewNode* allPatchesItem_;
	TreeViewNode* userListsItem_;
};

