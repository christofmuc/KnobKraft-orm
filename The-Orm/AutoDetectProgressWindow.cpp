/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "AutoDetectProgressWindow.h"

#include "UIModel.h"

void AutoDetectProgressWindow::run()
{
	std::vector < std::shared_ptr<midikraft::SimpleDiscoverableDevice>> synths;
	for (auto s : synths_) {
		if (UIModel::instance()->synthList_.isSynthActive(s.device())) {
			synths.push_back(s.device());
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

bool AutoDetectProgressWindow::shouldAbort() const
{
	return threadShouldExit();
}

void AutoDetectProgressWindow::setProgressPercentage(double zeroToOne)
{
	setProgress(zeroToOne);
}

void AutoDetectProgressWindow::onSuccess()
{
	// The detection state could be different, fire an update message
	UIModel::instance()->currentSynth_.sendChangeMessage();
}

void AutoDetectProgressWindow::onCancel()
{
}
