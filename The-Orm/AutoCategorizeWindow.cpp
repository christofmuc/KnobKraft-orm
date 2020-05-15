/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "AutoCategorizeWindow.h"

void AutoCategorizeWindow::run()
{
	// Load the auto category file and re-categorize everything!
	midikraft::AutoCategory::loadFromFile(fullPathToAutoCategoryFile_.toStdString());
	auto patches = database_->getPatches(activeFilter_, 0, 100000);
	size_t tick = 0;
	for (auto patch : patches) {
		if (patch.autoCategorizeAgain()) {
			if (threadShouldExit()) break;
			// This was changed, updating database
			SimpleLogger::instance()->postMessage("Updating patch " + String(patch.name()) + " with new categories");
			database_->putPatch(patch);
		}
		setProgress(tick++ / (double)patches.size());
	}
	MessageManager::callAsync([this]() {
		finishedHandler_();
	});
}
