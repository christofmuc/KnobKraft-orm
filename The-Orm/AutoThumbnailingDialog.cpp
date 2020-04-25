/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "AutoThumbnailingDialog.h"

#include "WaitForEvent.h"
#include "Logger.h"
#include "UIModel.h"


AutoThubnailingDialog::AutoThubnailingDialog(RecordingView &recordingView) : ThreadWithProgressWindow("Recording patch thumbnails", true, true), recordingView_(recordingView)
{
}

void AutoThubnailingDialog::run()
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
	while (!threadShouldExit()) {
		// For now, only record one patch
		bool done = false;
		recordingView_.sampleNote([&]() { done = true;  });

		WaitForEvent waiting([&]() { return done; }, this);
		waiting.startThread();
		if (wait(10000)) {
			return;
		}
		else {
			SimpleLogger::instance()->postMessage("No patch could be recorded, please check the Audio setup in the AudioInv view!");
			return;
		}
	}
}

