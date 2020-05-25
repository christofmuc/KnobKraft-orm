/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "JuceHeader.h"

#include "Synth.h"
#include "StepSequencer.h"
#include "PatchHolder.h"
#include "SynthHolder.h"
#include "Session.h"

class CurrentSynth : public ChangeBroadcaster {
public:
	void changeCurrentSynth(std::weak_ptr<midikraft::Synth> activeSynth);

	midikraft::Synth *synth() {
		return currentSynth_.expired() ? nullptr : currentSynth_.lock().get();
	}

	std::shared_ptr<midikraft::Synth> smartSynth() {
		return currentSynth_.expired() ? nullptr : currentSynth_.lock();
	}

private:
	std::weak_ptr<midikraft::Synth> currentSynth_;
};

class CurrentSequencer : public ChangeBroadcaster {
public:
	void changeCurrentSequencer(midikraft::StepSequencer *activeSequencer);

	midikraft::StepSequencer *sequencer() {
		return currentSequencer_;
	}

private:
	midikraft::StepSequencer *currentSequencer_ = nullptr;
};

class CurrentPatch : public ChangeBroadcaster {
public:
	void changeCurrentPatch(midikraft::PatchHolder const &currentPatch);

	midikraft::PatchHolder patch() {
		return currentPatch_;
	}

private:
	midikraft::PatchHolder currentPatch_;
};

class CurrentPatchValues : public ChangeBroadcaster {
public:
	void changedPatch();
};

class CurrentSession : public ChangeBroadcaster {
public:
	void changedSession();

	std::vector<std::shared_ptr<midikraft::SessionPatch>> session() { return sessionPatches;  }

private:
	std::vector<std::shared_ptr<midikraft::SessionPatch>> sessionPatches;
};

class CurrentSynthList : public ChangeBroadcaster {
public:
	void setSynthList(std::vector<midikraft::SynthHolder> const &synths);
	void setSynthActive(midikraft::SimpleDiscoverableDevice *synth, bool isActive);

	std::vector<midikraft::SynthHolder> allSynths();
	midikraft::SynthHolder synthByName(std::string const &name);
	std::vector<std::shared_ptr<midikraft::SimpleDiscoverableDevice>> activeSynths();
	bool isSynthActive(std::shared_ptr<midikraft::SimpleDiscoverableDevice> synth);

private:
	std::vector<std::pair<midikraft::SynthHolder, bool>> synths_;	
};

class UIModel {
public:
	static UIModel *instance();
	static void shutdown();

	static midikraft::Synth *currentSynth();
	static midikraft::Synth* currentSynthOfPatch();
	static midikraft::StepSequencer *currentSequencer();
	static midikraft::PatchHolder currentPatch();

	CurrentSynth currentSynth_; // Listen to this to get updated when the active synth is switched
	CurrentSequencer currentSequencer_;
	CurrentPatch currentPatch_; // Listen to this to get updated when the current patch changes
	CurrentPatchValues currentPatchValues_; // Listen to this to find out if the current patch was modified
	CurrentSession currentSession_; // Listen to this to find out if the current session was modified
	CurrentSynthList synthList_;

private:
	UIModel() {};

	static std::unique_ptr<UIModel> instance_;
};
