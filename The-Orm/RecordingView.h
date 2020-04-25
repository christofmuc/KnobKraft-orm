/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "JuceHeader.h"

#include "AudioRecorder.h"
#include "Thumbnail.h"
#include "TimedMidiSender.h"

#include "LambdaButtonStrip.h"

#include "PatchView.h"

class RecordingView : public Component, public ChangeBroadcaster, private ChangeListener {
public:
	RecordingView(PatchView &patchView);
	~RecordingView();

	void stopAudio();

	virtual void resized() override;

	void sampleNote();
	bool hasDetectedSignal() const;

private:
	void changeListenerCallback(ChangeBroadcaster* source) override;

	PatchView &patchView_;

	AudioDeviceManager deviceManager_;
	AudioDeviceSelectorComponent deviceSelector_;
	AudioSourcePlayer audioSource_;

	AudioRecorder recorder_;
	midikraft::TimedMidiSender midiSender_;

	LambdaButtonStrip buttons_;
	Thumbnail thumbnail_;
};

