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
	AutoCategorizeWindow(midikraft::PatchDatabase *database, String fullPathToAutoCategoryFile, midikraft::PatchDatabase::PatchFilter activeFilter, std::function<void()> finishedHandler) :
		ThreadWithProgressWindow("Re-running auto categorization...", true, true), database_(database), fullPathToAutoCategoryFile_(fullPathToAutoCategoryFile),
		activeFilter_(activeFilter), finishedHandler_(finishedHandler)
	{
	}

	void run();

private:
	midikraft::PatchDatabase *database_;
	String fullPathToAutoCategoryFile_;
	midikraft::PatchDatabase::PatchFilter activeFilter_;
	std::function<void()> finishedHandler_;
};

