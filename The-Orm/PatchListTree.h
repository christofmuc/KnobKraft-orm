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

	PatchListTree(midikraft::PatchDatabase &db, std::vector<midikraft::SynthHolder> const& synths);
	virtual ~PatchListTree();

	TSelectionHandler onImportListSelected;
	TSelectionHandler onUserListSelected;
	TSelectionHandler onUserListChanged;

	virtual void resized();

	void refreshAllUserLists();
	void refreshUserList(std::string list_id);
	void refreshAllImports();

	void selectItemByPath(std::vector<std::string> const& path);

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PatchListTree)
	
private:
	void regenerateUserLists();
	void regenerateImportLists();

	TreeViewItem* newTreeViewItemForPatch(midikraft::ListInfo list, midikraft::PatchHolder patchHolder);
	TreeViewItem* newTreeViewItemForPatchList(midikraft::ListInfo list);
	void changeListenerCallback(ChangeBroadcaster* source) override;

	std::map<std::string, std::weak_ptr<midikraft::Synth>> synths_; // The database needs this to load patch lists
	//std::map<std::string, std::unique_ptr<XmlElement>> synthSpecificTreeState_;

	midikraft::PatchDatabase& db_;

	std::unique_ptr<TreeView> treeView_;
	TreeViewNode* allPatchesItem_;
	TreeViewNode* importListsItem_;
	TreeViewNode* userListsItem_;
	std::map<std::string, TreeViewNode*> userLists_;
	std::string previousSynthName_;
};

