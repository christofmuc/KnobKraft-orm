/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "JuceHeader.h"

#include "AudioRecorder.h"

class RecordingView : public Component {
public:
	RecordingView();
	~RecordingView();

	void stopAudio();

	virtual void resized() override;

private:
	AudioDeviceManager deviceManager_;
	AudioDeviceSelectorComponent deviceSelector_;
	AudioSourcePlayer audioSource_;

	AudioRecorder recorder_;
};

