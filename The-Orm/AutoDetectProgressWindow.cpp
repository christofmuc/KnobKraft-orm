/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "AutoDetectProgressWindow.h"

void AutoDetectProgressWindow::run()
{
	std::vector < std::shared_ptr<midikraft::SimpleDiscoverableDevice>> synths;
	for (auto s : synths_) {
		synths.push_back(s.device());
	}
	autodetector_.autoconfigure(synths, this);
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
}

void AutoDetectProgressWindow::onCancel()
{
}
