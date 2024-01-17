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

class SimilarityIndex {
public:
	SimilarityIndex(std::shared_ptr<midikraft::Synth> synth);
	~SimilarityIndex();

	void addPatches(std::vector<midikraft::PatchHolder> const& allPatches);
	KArray* search(midikraft::PatchHolder const& patch);
	static Merkmal* patchToFeatureVector(int key, size_t dimension, midikraft::PatchHolder const& patch);


private:
	std::string vpFileName_;
	std::shared_ptr<midikraft::Synth> synth_;
	int dimensionality_;
};


class SimilarPatchesPanel : public Component, private ChangeListener
{
public:
	SimilarPatchesPanel(PatchView *patchView, midikraft::PatchDatabase &db);
	virtual ~SimilarPatchesPanel() override;

	virtual void resized() override;

private:
	virtual void changeListenerCallback(ChangeBroadcaster* source) override;

	PatchView* patchView_;
	midikraft::PatchDatabase& db_;
	PatchButtonInfo buttonMode_;
	std::unique_ptr<VerticalPatchButtonList> similarity_;
	std::shared_ptr<midikraft::PatchList> similarList_;
	std::unique_ptr<SimilarityIndex> activeIndex_;
	std::vector<midikraft::PatchHolder> allPatches_;
};
