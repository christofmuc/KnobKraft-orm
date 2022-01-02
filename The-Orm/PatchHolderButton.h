/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "PatchButton.h"
#include "PatchHolder.h"

class PatchHolderButton : public PatchButtonWithDropTarget {
public:
	using PatchButtonWithDropTarget::PatchButtonWithDropTarget;
	
	void setPatchHolder(midikraft::PatchHolder *holder, bool active, bool showSynthName);

	static Colour buttonColourForPatch(midikraft::PatchHolder &patch, Component *componentForDefaultBackground);
};

