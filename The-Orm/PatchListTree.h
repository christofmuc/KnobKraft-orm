/*
   Copyright (c) 2021 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "JuceHeader.h"

#include "PatchDatabase.h"

class PatchListTree : public Component {
public:

	PatchListTree(midikraft::PatchDatabase &db, std::function<void(String)> clickHandler);
	virtual ~PatchListTree();

	virtual void resized();

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PatchListTree)

private:
	midikraft::PatchDatabase& db_;
	std::function<void(String)> clickHandler_;

	std::unique_ptr<TreeView> treeView_;
};

