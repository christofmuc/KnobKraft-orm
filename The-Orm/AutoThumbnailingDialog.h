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
	AutoThumbnailingDialog(PatchView &patchView, RecordingView &recordingView);
	virtual ~AutoThumbnailingDialog();

	virtual void run() override;

private:
	void changeListenerCallback(ChangeBroadcaster* source) override;

	PatchView &patchView_;
	RecordingView &recordingView_;
	bool patchSwitched_;
	bool thumbnailDone_;
};

