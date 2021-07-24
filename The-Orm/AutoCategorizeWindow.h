/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "JuceHeader.h"

#include "AutomaticCategory.h"
#include "Logger.h"

#include "UIModel.h"
#include "PatchDatabase.h"

class AutoCategorizeWindow : public ThreadWithProgressWindow {
public:
	AutoCategorizeWindow(midikraft::PatchDatabase *database, std::shared_ptr<midikraft::AutomaticCategory> detector, midikraft::PatchFilter activeFilter, std::function<void()> finishedHandler) :
		ThreadWithProgressWindow("Re-running auto categorization...", true, true), database_(database), detector_(detector),
		activeFilter_(activeFilter), finishedHandler_(finishedHandler)
	{
	}

	virtual void run() override;

private:
	midikraft::PatchDatabase *database_;
	std::shared_ptr<midikraft::AutomaticCategory> detector_;
	midikraft::PatchFilter activeFilter_;
	std::function<void()> finishedHandler_;
};

