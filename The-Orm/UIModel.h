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

	[[deprecated]]
	midikraft::Synth *synth();

	std::shared_ptr<midikraft::Synth> smartSynth();

private:
	std::weak_ptr<midikraft::Synth> currentSynth_;
};

class CurrentMultiMode : public ChangeBroadcaster {
public:
	void setMultiSynthMode(bool multiMode);
	bool multiSynthMode() const;

private:
	bool multiSynthMode_;
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

	std::map<std::string, midikraft::PatchHolder> allCurrentPatches() const;

private:
	midikraft::PatchHolder currentPatch_;
	std::map<std::string, midikraft::PatchHolder> currentPatchBySynth_;
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

class ThumbnailChanges : public ChangeBroadcaster {
};

class WindowTitleChanges : public ChangeBroadcaster {
};

class UIModel {
public:
	static UIModel *instance();
	static void shutdown();

	static midikraft::Synth *currentSynth();
	static midikraft::Synth* currentSynthOfPatch();
	static std::shared_ptr<midikraft::Synth> currentSynthOfPatchSmart();
	static midikraft::StepSequencer *currentSequencer();
	static midikraft::PatchHolder currentPatch();

	static File getPrehearDirectory();
	static File getThumbnailDirectory();

	CurrentSynth currentSynth_; // Listen to this to get updated when the active synth is switched
	CurrentMultiMode multiMode_;
	CurrentSequencer currentSequencer_;
	CurrentPatch currentPatch_; // Listen to this to get updated when the current patch changes
	CurrentPatchValues currentPatchValues_; // Listen to this to find out if the current patch was modified
	CurrentSynthList synthList_;
	ThumbnailChanges thumbnails_;
	WindowTitleChanges windowTitle_;
	ChangeBroadcaster categoriesChanged; // Listen to this to get notified of category list changes
	ChangeBroadcaster databaseChanged; // Listen to this when you need to know a new database was opened

private:
	UIModel() = default;

	static std::unique_ptr<UIModel> instance_;
};
