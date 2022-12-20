/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "AutoDetectProgressWindow.h"

#include "UIModel.h"

AutoDetectProgressWindow::AutoDetectProgressWindow(std::vector<midikraft::SynthHolder> synths) :
	ProgressHandlerWindow("Running auto-detection", "Detecting synth...")
{
	for (auto s : synths) {
		if (UIModel::instance()->synthList_.isSynthActive(s.device())) {
			synths_.push_back(s.device());
		}
	}
}

AutoDetectProgressWindow::AutoDetectProgressWindow(std::vector<std::shared_ptr<midikraft::SimpleDiscoverableDevice>> synths) :
	ProgressHandlerWindow("Running auto-detection", "Detecting synth...")
{
	for (auto synth : synths) {
		synths_.push_back(synth); // Convert from shared to weak_ptr
	}
}

void AutoDetectProgressWindow::run()
{
	std::vector<std::shared_ptr<midikraft::SimpleDiscoverableDevice>> synths;
	for (auto synth : synths_) {
		if (!synth.expired()) {
			synths.push_back(synth.lock());
		}
	}
	autodetector_.autoconfigure(synths, this);
	if (!shouldAbort()) {
		onSuccess();
	}
	else {
		onCancel();
	}
}

void AutoDetectProgressWindow::onSuccess()
{
	// The detection state could be different, fire an update message
	UIModel::instance()->currentSynth_.sendChangeMessage();
}

void AutoDetectProgressWindow::onCancel()
{
	//Forgot why, but we should not signal the thread to exit as in the default implementation of ProgressHandlerWindow
}
