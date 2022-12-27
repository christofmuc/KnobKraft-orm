/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "JuceHeader.h"

#include "PatchView.h"
#include "RecordingView.h"

class AutoThumbnailingDialog : public ThreadWithProgressWindow, private ChangeListener {
public:
	AutoThumbnailingDialog(RecordingView &recordingView);
	virtual ~AutoThumbnailingDialog() override;

	virtual void run() override;

private:
	bool syncSwitchToNextPatch();
	bool waitForPatchSwitchAndSendToSynth();
	bool syncSwitchToFirstPatch();
	bool syncRecordThumbnail();
	void changeListenerCallback(ChangeBroadcaster* source) override;

	RecordingView &recordingView_;
	bool patchSwitched_;
	bool thumbnailDone_;
};

