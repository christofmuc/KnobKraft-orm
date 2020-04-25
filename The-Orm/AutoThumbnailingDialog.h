/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "JuceHeader.h"

#include "RecordingView.h"

class AutoThubnailingDialog : public ThreadWithProgressWindow {
public:
	AutoThubnailingDialog(RecordingView &recordingView);

	virtual void run() override;

private:
	RecordingView &recordingView_;
};

