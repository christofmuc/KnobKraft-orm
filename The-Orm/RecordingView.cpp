/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "RecordingView.h"

#include "Logger.h"
#include "MidiNote.h"
#include "UIModel.h"

RecordingView::RecordingView() : deviceSelector_(deviceManager_, 1, 2, 1, 1, false, false, true, false),
	recorder_(File::getCurrentWorkingDirectory(), "knobkraft-audio-log", RecordingType::WAV), buttons_(1111, LambdaButtonStrip::Direction::Horizontal),
	midiSender_(48000)
{
	addAndMakeVisible(deviceSelector_);

	auto audioError = deviceManager_.initialise(1, 0, nullptr, true);
	if (!audioError.isEmpty()) {
		SimpleLogger::instance()->postMessage("Error initializing audio device manager: " + audioError);
	}
	recorder_.setRecording(false, []() {});
	deviceManager_.addAudioCallback(&recorder_);

	addAndMakeVisible(thumbnail_);

	LambdaButtonStrip::TButtonMap buttons = {
	{ "performSample", { 0, "Sample one note", [this]() {
		sampleNote();
	} } },
	};
	buttons_.setButtonDefinitions(buttons);
	addAndMakeVisible(buttons_);

	thumbnail_.addChangeListener(this);
}

RecordingView::~RecordingView()
{
	thumbnail_.removeChangeListener(this);
	stopAudio();
}

void RecordingView::stopAudio() {
	deviceManager_.removeAudioCallback(&recorder_);
}

void RecordingView::resized()
{
	auto area = getLocalBounds();

	thumbnail_.setBounds(area.removeFromBottom(100).reduced(8));
	buttons_.setBounds(area.removeFromBottom(60).reduced(8));
	deviceSelector_.setBounds(area.reduced(8));
}

void RecordingView::sampleNote() {
	// Ok, what we'll do is to 
	// a) start the recorder to listen for audio coming in
	// b) send a MIDI note to the current synth
	// register a callback that the recorder will call when the signal is done, and then refresh the Thumbnail
	recorder_.setRecording(true, [this]() {
		String filename = recorder_.getFilename();
		thumbnail_.loadFromFile(filename.toStdString());
	});

	auto currentChannel = UIModel::instance()->currentSynth()->channel();
	if (currentChannel.isValid()) {
		auto noteNumber = MidiNote(440.0).noteNumber();
		auto noteOn = MidiMessage::noteOn(currentChannel.toOneBasedInt(), noteNumber, (uint8) 127);
		auto noteOff = MidiMessage::noteOff(currentChannel.toOneBasedInt(), noteNumber);
		midiSender_.addMessageToBuffer(UIModel::currentSynth()->midiOutput(), noteOn, 0.0);
		midiSender_.addMessageToBuffer(UIModel::currentSynth()->midiOutput(), noteOff, 0.5);
	}
}

void RecordingView::changeListenerCallback(ChangeBroadcaster* source)
{
	ignoreUnused(source);
	thumbnail_.repaint();
}
