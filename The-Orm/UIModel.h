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
#include "SynthBank.h"

#include "Data.h"

juce::Identifier const PROPERTY_SYNTH_LIST {"SynthList"};
juce::Identifier const PROPERTY_BUTTON_INFO_TYPE {"ButtonInfoType"};
juce::Identifier const PROPERTY_COMBOBOX_SENDMODE {"SynthSendMode"};
juce::Identifier const PROPERTY_WINDOW_LIST {"Windows"};
juce::Identifier const PROPERTY_WINDOW_OPENNESS {"Open"};
juce::Identifier const PROPERTY_WINDOW_SIZE {"Size"};

// The EphemeralData is not stored on disk, but needs to be cleared via the UIModel clear method when the database changes.
juce::Identifier const EPROPERTY_LIBRARY_PATCH_LIST {"LibraryPatchList"};


juce::Identifier const EPROPERTY_MIDI_LOG_LEVEL{ "MIDILog" };


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

class CurrentSynthList : public ChangeBroadcaster {
public:
	CurrentSynthList() : synths_() {
	};

	virtual ~CurrentSynthList() = default;

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

	void clear();

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
	ChangeBroadcaster importListChanged_; // Listen to this get refresh the list of imports
	CurrentSynthList synthList_;
	ThumbnailChanges thumbnails_;
	WindowTitleChanges windowTitle_;
	ChangeBroadcaster categoriesChanged; // Listen to this to get notified of category list changes
	ChangeBroadcaster databaseChanged; // Listen to this when you need to know a new database was opened

	static std::string currentSynthNameOrMultiOrEmpty(); // Get the name of the current Synth, or "multiMode" if activated, or empty if no synth
	static ValueTree ensureSynthSpecificPropertyExists(std::string const& synthName, juce::Identifier const& property, var const& defaultValue);
	static Value getSynthSpecificPropertyAsValue(std::string const& synthName, juce::Identifier const& property, var const& defaultValue);

private:
	UIModel() = default;

	static std::unique_ptr<UIModel> instance_;
};
