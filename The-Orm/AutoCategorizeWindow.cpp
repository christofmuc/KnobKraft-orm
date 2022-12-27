/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "AutoCategorizeWindow.h"

#include <spdlog/spdlog.h>

void AutoCategorizeWindow::run()
{
	// Load the auto category file and re-categorize everything!
	if (detector_->autoCategoryFileExists()) {
		detector_->loadFromFile(database_.getCategories(), detector_->getAutoCategoryFile().getFullPathName().toStdString());
	}
	auto patches = database_.getPatches(activeFilter_, 0, 100000);
	size_t tick = 0;
	for (auto patch : patches) {
		if (patch.autoCategorizeAgain(detector_)) {
			if (threadShouldExit()) break;
			// This was changed, updating database
			spdlog::info("Updating patch {} with new categories", patch.name());
			database_.putPatch(patch);
		}
		setProgress(tick++ / (double)patches.size());
	}
	MessageManager::callAsync([this]() {
		finishedHandler_();
	});
}
