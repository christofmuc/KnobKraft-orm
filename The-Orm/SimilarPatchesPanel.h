/*
   Copyright (c) 2022 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "JuceHeader.h"

#include "VerticalPatchButtonList.h"
#include "InfoText.h"
#include "PatchHolder.h"

#include "Similarity.h"
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
	void runSearch();

	PatchView* patchView_;
	midikraft::PatchDatabase& db_;
	PatchButtonInfo buttonMode_;
	TextEditor helpText_;
	Label metricsLabel_;
	TextButton l2_;
	TextButton ip_;
	Label similarityLabel_;
	Slider similarityValue_;
	std::unique_ptr<VerticalPatchButtonList> similarity_;
	std::shared_ptr<midikraft::PatchList> similarList_;
	std::unique_ptr<PatchSimilarity> activeIndex_;
	std::vector<midikraft::PatchHolder> allPatches_;
};
