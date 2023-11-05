/*
   Copyright (c) 2022 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "JuceHeader.h"

#include "VerticalPatchButtonList.h"
#include "InfoText.h"
#include "PatchHolder.h"

#include "VPBaum.hh"
#include "PatchDatabase.h"

class PatchView;

class SimilarPatchesPanel : public Component, private ChangeListener
{
public:
	SimilarPatchesPanel(PatchView *patchView, midikraft::PatchDatabase &db);
	virtual ~SimilarPatchesPanel() override;

	virtual void resized() override;

private:
	virtual void changeListenerCallback(ChangeBroadcaster* source) override;
	Merkmal *patchToFeatureVector(int key, midikraft::PatchHolder const& patch);

	PatchView* patchView_;
	midikraft::PatchDatabase& db_;
	PatchButtonInfo buttonMode_;
	std::unique_ptr<VerticalPatchButtonList> similarity_;
	std::shared_ptr<midikraft::PatchList> similarList_;
};
