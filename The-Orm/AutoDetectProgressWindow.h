/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "JuceHeader.h"

#include "ProgressHandlerWindow.h"
#include "SynthHolder.h"
#include "AutoDetection.h"

class AutoDetectProgressWindow : public ProgressHandlerWindow {
public:
	AutoDetectProgressWindow(std::vector<midikraft::SynthHolder> synths) :
		ProgressHandlerWindow("Running auto-detection", "Detecting synth..."), synths_(synths) {
	}

	// Implement ThreadWithProgressWindow
	virtual void run() override;

	// Implement ProgressHandler interface
	virtual void onSuccess() override;
	virtual void onCancel() override;

private:
	std::vector<midikraft::SynthHolder> synths_;
	midikraft::AutoDetection autodetector_;
};


