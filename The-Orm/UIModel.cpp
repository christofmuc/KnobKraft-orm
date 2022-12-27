/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "UIModel.h"

#include "FileHelpers.h"
#include "Data.h"

void CurrentSynth::changeCurrentSynth(std::weak_ptr<midikraft::Synth> activeSynth)
{
	currentSynth_ = activeSynth;
	sendChangeMessage();
}

midikraft::Synth* CurrentSynth::synth()
{
	return currentSynth_.expired() ? nullptr : currentSynth_.lock().get();
}

std::shared_ptr<midikraft::Synth> CurrentSynth::smartSynth()
{
	return currentSynth_.expired() ? nullptr : currentSynth_.lock();
}

void CurrentSequencer::changeCurrentSequencer(midikraft::StepSequencer *activeSequencer)
{
	currentSequencer_ = activeSequencer;
	sendChangeMessage();
}


void CurrentPatch::changeCurrentPatch(midikraft::PatchHolder const& currentPatch) {
	currentPatch_ = std::make_shared<midikraft::PatchHolder>(currentPatch);
	changeCurrentPatch(currentPatch_);
}

void CurrentPatch::changeCurrentPatch(std::shared_ptr<midikraft::PatchHolder> currentPatch)
{
	currentPatch_ = currentPatch;
	currentPatchBySynth_[currentPatch->synth()->getName()] = currentPatch;
	sendChangeMessage();
}

std::map<std::string, std::shared_ptr<midikraft::PatchHolder>> CurrentPatch::allCurrentPatches() const
{
	return currentPatchBySynth_;
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
	return instance_->currentSynth_.smartSynth().get();
}

midikraft::Synth* UIModel::currentSynthOfPatch()
{
	if (instance_->currentPatch_.patch()->patch()) {
		return instance_->currentPatch()->synth();
	}
	return currentSynth();
}

std::shared_ptr<midikraft::Synth> UIModel::currentSynthOfPatchSmart()
{
	if (instance_->currentPatch_.patch()->patch()) {
		return instance_->currentPatch()->smartSynth();
	}
	return instance_->currentSynth_.smartSynth();
}

midikraft::StepSequencer * UIModel::currentSequencer()
{
	return instance_->currentSequencer_.sequencer();
}

std::shared_ptr<midikraft::PatchHolder> UIModel::currentPatch()
{
	return instance_->currentPatch_.patch();
}

juce::File UIModel::getPrehearDirectory()
{
	auto knobkraftorm = getOrCreateSubdirectory(File::getSpecialLocation(File::userApplicationDataDirectory), "KnobKraftOrm"); //TODO this should be a define?
	return getOrCreateSubdirectory(knobkraftorm, "PatchPrehear");
}

juce::File UIModel::getThumbnailDirectory()
{
	auto knobkraftorm = getOrCreateSubdirectory(File::getSpecialLocation(File::userApplicationDataDirectory), "KnobKraftOrm"); //TODO this should be a define?
	return getOrCreateSubdirectory(knobkraftorm, "PatchThumbnails");
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

void CurrentSynthBank::setSynthBank(std::shared_ptr<midikraft::SynthBank> synthBank) 
{
	synthBank_ = synthBank;
	modified_ = false;
	sendChangeMessage();
}

std::shared_ptr<midikraft::SynthBank> CurrentSynthBank::synthBank()
{
	return synthBank_;
}

void CurrentSynthBank::flagModified()
{
	modified_ = true;
	sendChangeMessage();
}

void CurrentSynthBank::clearModified()
{
	modified_ = false;
	sendChangeMessage();
}

void CurrentMultiMode::setMultiSynthMode(bool multiMode)
{
	multiSynthMode_ = multiMode;
	sendChangeMessage();
}

bool CurrentMultiMode::multiSynthMode() const
{
	return multiSynthMode_;
}

ValueTree UIModel::ensureSynthSpecificPropertyExists(std::string const& synthName, juce::Identifier const& property, var const& defaultValue) {
	
	auto synths = Data::instance().get().getOrCreateChildWithName(PROPERTY_SYNTH_LIST, nullptr);
	auto synth = synths.getChildWithProperty("synthName", synthName.c_str());
	if (!synth.isValid()) {
		synth = ValueTree("Synth");
		synth.setProperty("synthName", synthName.c_str(), nullptr);
		synths.addChild(synth, -1, nullptr);
	}
	if (!synth.hasProperty(property)) {
		synth.setProperty(property, defaultValue, nullptr);
	}
	return synth;
}

Value UIModel::getSynthSpecificPropertyAsValue(std::string const& synthName, juce::Identifier const& property, var const& defaultValue) 
{
	auto synth = ensureSynthSpecificPropertyExists(synthName, property, defaultValue);
	return synth.getPropertyAsValue(property, nullptr);
}

