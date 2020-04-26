/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "AutoThumbnailingDialog.h"

#include "WaitForEvent.h"
#include "Logger.h"
#include "UIModel.h"


AutoThumbnailingDialog::AutoThumbnailingDialog(PatchView &patchView, RecordingView &recordingView) : 
	ThreadWithProgressWindow("Recording patch thumbnails", true, true), patchView_(patchView), recordingView_(recordingView)
{
	UIModel::instance()->currentPatch_.addChangeListener(this);
	recordingView_.addChangeListener(this);
}

AutoThumbnailingDialog::~AutoThumbnailingDialog()
{
	UIModel::instance()->currentPatch_.removeChangeListener(this);
	recordingView_.removeChangeListener(this);
}

bool AutoThumbnailingDialog::syncSwitchToNextPatch() {
	// Select the next patch. This is asynchronous, because the database might need to load
	patchSwitched_ = false;
	PatchView *patchView = &patchView_;
	MessageManager::callAsync([patchView]() {
		patchView->selectNextPatch();
	});
	return waitForPatchSwitchAndSendToSynth();
}

bool AutoThumbnailingDialog::waitForPatchSwitchAndSendToSynth() {
	WaitForEvent waiter1([this]() { return patchSwitched_;  }, this);
	waiter1.startThread();
	if (!wait(1000)) {
		waiter1.stopThread(1000);
		// No more patches!
		return false;
	}

	// Now, each synth needs a different amount of time to process the new patch and be able to play the first note with it
	// let's use the detection interval as a hint
	auto synth = UIModel::currentSynth();
	sleep(synth->deviceDetectSleepMS());
	return true;
}

bool AutoThumbnailingDialog::syncSwitchToFirstPatch() {
	// Select the next patch. This is asynchronous, because the database might need to load
	patchSwitched_ = false;
	PatchView *patchView = &patchView_;
	MessageManager::callAsync([patchView]() {
		patchView->selectFirstPatch();
	});
	return waitForPatchSwitchAndSendToSynth();
}


bool AutoThumbnailingDialog::syncRecordThumbnail() {
	// Record the current patch
	thumbnailDone_ = false;
	recordingView_.sampleNote();

	// First check that we can actually record a signal, that should be quick
	WaitForEvent waitingForSignal([&]() { return recordingView_.hasDetectedSignal(); }, this);
	WaitForEvent waitingForDone([this]() { return thumbnailDone_; }, this);
	waitingForSignal.startThread();
	if (wait(5000)) {
		waitingForDone.startThread();
		if (wait(60000)) {
			// What do we do in case of drones?
		}
		else {
			waitingForDone.stopThread(1000);
			SimpleLogger::instance()->postMessage("That was a never ending patch - you're sure you're not recording something else?");
			return false;
		}
	}
	else {
		waitingForSignal.stopThread(1000);
		SimpleLogger::instance()->postMessage("No patch could be recorded, please check the Audio setup in the AudioIn view!");
		return false;
	}
	return true;
}

void AutoThumbnailingDialog::run()
{
	// We need a current Synth, and that needs to be detected successfully!
	auto synth = UIModel::currentSynth();
	if (!synth) {
		jassertfalse; // This would be a program error
		return; 
	}

	if (!synth->channel().isValid()) {
		SimpleLogger::instance()->postMessage("Cannot record patches when the " + synth->getName() + " hasn't been detected!");
		return;
	}

	// Loop over all selected patches and record the thumbnails!
	if (syncSwitchToFirstPatch()) {
		int recordedCount = 1;
		int toRecord = patchView_.totalNumberOfPatches();
		while (!threadShouldExit()) {
			// Record current patch
			if (!syncRecordThumbnail()) {
				break;
			}

			// Switch to next Patch
			if (!syncSwitchToNextPatch()) {
				// That didn't work, no more patches
				break;
			}

			setProgress(recordedCount++ / (double)toRecord);
		}
	}
}

void AutoThumbnailingDialog::changeListenerCallback(ChangeBroadcaster* source)
{
	if (source == &UIModel::instance()->currentPatch_) {
		patchSwitched_ = true;
	}
	else if (source == &recordingView_) {
		thumbnailDone_ = true;
	}
}

