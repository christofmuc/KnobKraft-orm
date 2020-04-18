/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "UIModel.h"

void CurrentSynth::changeCurrentSynth(midikraft::Synth *activeSynth)
{
	currentSynth_ = activeSynth;
	sendChangeMessage();
}


void CurrentSequencer::changeCurrentSequencer(midikraft::StepSequencer *activeSequencer)
{
	currentSequencer_ = activeSequencer;
	sendChangeMessage();
}


void CurrentPatch::changeCurrentPatch(midikraft::PatchHolder const &currentPatch)
{
	currentPatch_ = currentPatch;
	sendChangeMessage();
}

void CurrentPatchValues::changedPatch()
{
	sendChangeMessage();
}

void CurrentSession::changedSession()
{
	sendChangeMessage();
}

UIModel * UIModel::instance()
{
	if (instance_ == nullptr) {
		instance_.reset(new UIModel());
	}
	return instance_.get();
}

void UIModel::shutdown()
{
	instance_.reset();
}

midikraft::Synth * UIModel::currentSynth()
{
	return instance_->currentSynth_.synth();
}

midikraft::StepSequencer * UIModel::currentSequencer()
{
	return instance_->currentSequencer_.sequencer();
}

midikraft::PatchHolder  UIModel::currentPatch()
{
	return instance_->currentPatch_.patch();
}

std::unique_ptr<UIModel> UIModel::instance_;

void CurrentSynthList::setSynthList(std::vector<midikraft::SynthHolder> const &synths)
{
	synths_ = synths;
	sendChangeMessage();
}

std::vector<midikraft::SynthHolder> CurrentSynthList::allSynths()
{
	return synths_;
}

std::vector<std::shared_ptr<midikraft::SimpleDiscoverableDevice>> CurrentSynthList::activeSynths()
{
	std::vector<std::shared_ptr<midikraft::SimpleDiscoverableDevice>> result;
	for (auto synth : synths_) {
		if (!synth.device()) continue;
		result.push_back(synth.device());
	}
	return result;
}
