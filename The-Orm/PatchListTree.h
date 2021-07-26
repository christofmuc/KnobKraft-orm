/*
   Copyright (c) 2021 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "JuceHeader.h"

#include "PatchDatabase.h"
#include "SynthHolder.h"

class PatchListTree : public Component, private ChangeListener {
public:
	typedef std::function<void(String)> TSelectionHandler;

	PatchListTree(midikraft::PatchDatabase &db, std::vector<midikraft::SynthHolder> const& synths, TSelectionHandler importListHandler, TSelectionHandler userListHandler);
	virtual ~PatchListTree();


	virtual void resized();

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PatchListTree)
	
private:
	void regenerateUserLists();

	TreeViewItem* newTreeViewItemForPatch(midikraft::PatchHolder patchHolder);
	TreeViewItem* newTreeViewItemForPatchList(midikraft::ListInfo list);
	void changeListenerCallback(ChangeBroadcaster* source) override;

	std::map<std::string, std::weak_ptr<midikraft::Synth>> synths_; // The database needs this to load patch lists
	std::map<std::string, std::unique_ptr<XmlElement>> synthSpecificTreeState_;

	midikraft::PatchDatabase& db_;
	TSelectionHandler importListHandler_, userListHandler_;

	std::unique_ptr<TreeView> treeView_;
	TreeViewItem* allPatchesItem_;
	TreeViewItem* userListsItem_;
	std::string previousSynthName_;
};

