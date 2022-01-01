/*
   Copyright (c) 2022 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "Librarian.h"
#include "ProgressHandlerWindow.h"

class LibrarianProgressWindow : public ProgressHandlerWindow {
public:
	LibrarianProgressWindow(midikraft::Librarian& librarian) : ProgressHandlerWindow("Import patches from Synth", "..."), librarian_(librarian) {
	}

	// Override this from the ThreadWithProgressWindow to understand closing with cancel button!
	virtual void threadComplete(bool userPressedCancel) override {
		if (userPressedCancel) {
			// Make sure to destroy any stray MIDI callback handlers, else we'll get into trouble when we retry the operation
			librarian_.clearHandlers();
		}
	}

private:
	midikraft::Librarian& librarian_;
};


