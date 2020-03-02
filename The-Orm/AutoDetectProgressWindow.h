/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "JuceHeader.h"

#include "ProgressHandler.h"
#include "SynthHolder.h"
#include "AutoDetection.h"

class AutoDetectProgressWindow : public ThreadWithProgressWindow, public midikraft::ProgressHandler {
public:
	AutoDetectProgressWindow(std::vector<midikraft::SynthHolder> synths) :
		ThreadWithProgressWindow("Detecting synth...", true, true), synths_(synths) {
	}

	// Implement ThreadWithProgressWindow
	void run();

	// Implement ProgressHandler interface
	virtual bool shouldAbort() const override;
	virtual void setProgressPercentage(double zeroToOne) override;
	virtual void onSuccess() override;
	virtual void onCancel() override;

private:
	std::vector<midikraft::SynthHolder> synths_;
	midikraft::AutoDetection autodetector_;
};


