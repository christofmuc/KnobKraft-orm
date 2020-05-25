/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "UIModel.h"

void CurrentSynth::changeCurrentSynth(std::weak_ptr<midikraft::Synth> activeSynth)
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

midikraft::Synth* UIModel::currentSynthOfPatch()
{
	if (instance_->currentPatch_.patch().patch()) {
		return instance_->currentPatch().synth();
	}
	return currentSynth();
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
	synths_.clear();
	for (auto synth : synths) {
		synths_.emplace_back(synth, true);
	}
	sendChangeMessage();
}

void CurrentSynthList::setSynthActive(midikraft::SimpleDiscoverableDevice *synth, bool isActive)
{
	for (auto &s : synths_) {
		if (!s.first.device()) continue;
		if (s.first.device()->getName() == synth->getName()) {
			s.second = isActive;
			sendChangeMessage();
			return;
		}
	}
	jassert(false);
}

std::vector<midikraft::SynthHolder> CurrentSynthList::allSynths()
{
	std::vector<midikraft::SynthHolder> result;
	for (auto synth : synths_) {
		result.push_back(synth.first);
	}
	return result;
}

midikraft::SynthHolder CurrentSynthList::synthByName(std::string const &name)
{
	for (auto synth : synths_) {
		if (synth.first.device() && synth.first.device()->getName() == name) {
			return synth.first;
		}
		else if (synth.first.synth() && synth.first.synth()->getName() == name) {
			return synth.first;
		}
	}
	return midikraft::SynthHolder(nullptr);
}

std::vector<std::shared_ptr<midikraft::SimpleDiscoverableDevice>> CurrentSynthList::activeSynths()
{
	std::vector<std::shared_ptr<midikraft::SimpleDiscoverableDevice>> result;
	for (auto synth : synths_) {
		if (!synth.second) continue;
		if (!synth.first.device()) continue;
		result.push_back(synth.first.device());
	}
	return result;
}

bool CurrentSynthList::isSynthActive(std::shared_ptr<midikraft::SimpleDiscoverableDevice> synth)
{
	for (auto s : activeSynths()) {
		if (s->getName() == synth->getName()) {
			return true;
		}
	}
	return false;
}
