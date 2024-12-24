/*
   Copyright (c) 2022 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "JuceHeader.h"

#include "VerticalPatchButtonList.h"
#include "InfoText.h"

class PatchView;

class PatchHistoryPanel : public Component, private ChangeListener
{
public:
	PatchHistoryPanel(PatchView *patchView);
	virtual ~PatchHistoryPanel() override;

	virtual void resized() override;

private:
	virtual void changeListenerCallback(ChangeBroadcaster* source) override;

	PatchView* patchView_;
	PatchButtonInfo buttonMode_;
	std::unique_ptr<VerticalPatchButtonList> history_;
	std::shared_ptr<midikraft::PatchList> patchHistory_;
};
