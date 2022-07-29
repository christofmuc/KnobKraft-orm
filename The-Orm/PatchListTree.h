/*
   Copyright (c) 2021 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "JuceHeader.h"

#include "PatchDatabase.h"
#include "SynthHolder.h"
#include "TreeViewNode.h"

class PatchListTree : public Component, private ChangeListener {
public:
	typedef std::function<void(String)> TSelectionHandler;
	typedef std::function<void(std::shared_ptr<midikraft::Synth>, MidiBankNumber)> TBankSelectionHandler;
	typedef std::function<void(midikraft::PatchHolder)> TPatchSelectionHandler;

	PatchListTree(midikraft::PatchDatabase &db, std::vector<midikraft::SynthHolder> const& synths);
	virtual ~PatchListTree() override;

	TSelectionHandler onImportListSelected;
	TBankSelectionHandler onSynthBankSelected;
	TSelectionHandler onUserListSelected;
	TSelectionHandler onUserListChanged;

	TPatchSelectionHandler onPatchSelected;

	virtual void resized() override;

	void refreshAllUserLists();
	void refreshUserList(std::string list_id);
	void refreshAllImports();

	void selectAllIfNothingIsSelected();
	void selectItemByPath(std::vector<std::string> const& path);

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PatchListTree)
	
private:
	void regenerateUserLists();
	void regenerateImportLists();

	void selectSynthLibrary(std::string const& synthName);
	std::string getSelectedSynth() const;
	bool isUserListSelected() const;
	std::list<std::string> pathOfSelectedItem() const;

	TreeViewItem* newTreeViewItemForPatch(midikraft::ListInfo list, midikraft::PatchHolder patchHolder, int index);
	TreeViewItem* newTreeViewItemForSynthBanks(std::shared_ptr<midikraft::SimpleDiscoverableDevice> synth);
	TreeViewItem* newTreeViewItemForImports(std::shared_ptr<midikraft::SimpleDiscoverableDevice> synth);
	TreeViewItem* newTreeViewItemForPatchList(midikraft::ListInfo list);

	void changeListenerCallback(ChangeBroadcaster* source) override;

	std::map<std::string, std::weak_ptr<midikraft::Synth>> synths_; // The database needs this to load patch lists
	//std::map<std::string, std::unique_ptr<XmlElement>> synthSpecificTreeState_;

	midikraft::PatchDatabase& db_;

	std::unique_ptr<TreeView> treeView_;
	TreeViewNode* allPatchesItem_;
	TreeViewNode* userListsItem_;
	std::map<std::string, TreeViewNode*> userLists_;
};

