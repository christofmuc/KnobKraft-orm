/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "RecordingView.h"

#include "Logger.h"

RecordingView::RecordingView() : deviceSelector_(deviceManager_, 1, 2, 1, 1, false, false, true, false),
	recorder_(File("."), "knobkraft-audio-log", RecordingType::WAV)
{
	addAndMakeVisible(deviceSelector_);

	auto audioError = deviceManager_.initialise(1, 0, nullptr, true);
	if (!audioError.isEmpty()) {
		SimpleLogger::instance()->postMessage("Error initializing audio device manager: " + audioError);
	}

	deviceManager_.addAudioCallback(&recorder_);
}

RecordingView::~RecordingView()
{
	stopAudio();
}

void RecordingView::stopAudio() {
	deviceManager_.removeAudioCallback(&recorder_);
}

void RecordingView::resized()
{
	auto area = getLocalBounds();

	deviceSelector_.setBounds(area.reduced(8));
}

