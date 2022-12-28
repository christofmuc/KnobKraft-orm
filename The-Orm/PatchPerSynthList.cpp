/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "PatchPerSynthList.h"

#include "UIModel.h"

PatchPerSynthList::PatchPerSynthList()
{
	UIModel::instance()->currentPatch_.addChangeListener(this);
}

PatchPerSynthList::~PatchPerSynthList()
{
	UIModel::instance()->currentPatch_.removeChangeListener(this);
}

void PatchPerSynthList::resized()
{
	Rectangle<int> area(getLocalBounds());
	int width = patchButtons_.size() != 0 ? std::min(area.getWidth() / (int) patchButtons_.size(), 150) : 0;

	auto activeArea = area.removeFromRight(width * (int) patchButtons_.size());
	for (auto &button : patchButtons_) {
		button->setBounds(activeArea.removeFromLeft(width).withTrimmedRight(8));
	}
}

void PatchPerSynthList::setPatches(std::vector<midikraft::PatchHolder> const &patches)
{
	patchButtons_.clear();
	buttonForSynth_.clear();
	int i = 0;
	for (auto patch : patches) {
		patchButtons_.push_back(std::make_shared<PatchHolderButton>(i++, false, [](int) {
		}));
		if (patch.patch() && patch.synth()) {
			patchButtons_.back()->setPatchHolder(patch, false, PatchHolderButton::getCurrentInfoForSynth(patch.synth()->getName()));
		}
		else {
			patchButtons_.back()->clearButton();
		}
		addAndMakeVisible(*patchButtons_.back());
		if (patch.synth()) {
			buttonForSynth_[patch.synth()->getName()] = patchButtons_.back();
		}
	}
	resized();
}

void PatchPerSynthList::changeListenerCallback(ChangeBroadcaster* source)
{
	if (source == &UIModel::instance()->currentPatch_) {
		// The Current Patch has changed - update our display!
		auto synthName = UIModel::currentPatch()->synth()->getName();
		if (buttonForSynth_.find(synthName) != buttonForSynth_.end()) {
			auto tempHolder = UIModel::currentPatch();
			if (tempHolder) {
				buttonForSynth_[synthName]->setPatchHolder(*tempHolder, false, PatchHolderButton::getCurrentInfoForSynth(synthName));
			}
			else {
				buttonForSynth_[synthName]->clearButton();
			}
		}
		else {
			//jassertfalse;
		}
	}
}
 