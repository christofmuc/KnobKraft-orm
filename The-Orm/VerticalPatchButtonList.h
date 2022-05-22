/*
   Copyright (c) 2022 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once


#include "PatchHolder.h"
#include "PatchHolderButton.h"

class VerticalPatchButtonList : public Component {
public:
	VerticalPatchButtonList(bool isDropTarget);

	virtual void resized() override;

	void setPatches(std::vector<midikraft::PatchHolder> const& patches, PatchButtonInfo info);

private:
	ListBox list_;
};
