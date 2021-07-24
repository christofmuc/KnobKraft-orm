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

	PatchListTree(midikraft::PatchDatabase &db, std::vector<midikraft::SynthHolder> const& synths, std::function<void(String)> clickHandler);
	virtual ~PatchListTree();

	virtual void resized();

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PatchListTree)
	
private:
	std::map<std::string, std::weak_ptr<midikraft::Synth>> synths_; // The database needs this to load patch lists
	TreeViewItem* newTreeViewItemForPatch(midikraft::PatchHolder patchHolder);
	TreeViewItem* newTreeViewItemForPatchList(midikraft::ListInfo list);
	void changeListenerCallback(ChangeBroadcaster* source) override;

	midikraft::PatchDatabase& db_;
	std::function<void(String)> clickHandler_;

	std::unique_ptr<TreeView> treeView_;
};

