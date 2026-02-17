/*
   Copyright (c) 2023 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "JuceHeader.h"

#include "PatchButtonPanel.h"
#include "PatchHolder.h"
#include "LambdaValueListener.h"

class PatchView;

class SimplePatchGrid : public Component
{
public:
	SimplePatchGrid(PatchView *patchView);
	~SimplePatchGrid() override;

	void resized() override;
	void applyPatchUpdate(midikraft::PatchHolder const& patch);

	std::function<void(midikraft::PatchHolder&)> onPatchSelected;

private:
	void reload();
	void patchSelectedHandler(midikraft::PatchHolder& patch);

	PatchView* patchView_;
	std::unique_ptr<PatchButtonPanel> grid_;
	ListenerSet listeners_;
};
