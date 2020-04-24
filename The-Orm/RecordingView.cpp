/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "RecordingView.h"

#include "Logger.h"
#include "MidiNote.h"
#include "UIModel.h"
#include "Settings.h"

RecordingView::RecordingView() : deviceSelector_(deviceManager_, 1, 2, 1, 1, false, false, true, false),
	recorder_(File::getCurrentWorkingDirectory(), "knobkraft-audio-log", RecordingType::WAV), buttons_(1111, LambdaButtonStrip::Direction::Horizontal),
	midiSender_(48000)
{
	addAndMakeVisible(deviceSelector_);

	// Load last setup
	std::string xmlString = Settings::instance().get("audioSetup", "");
	std::unique_ptr<XmlElement> xml;
	if (!xmlString.empty()) {
		// Try to parse this as XML
		xml = XmlDocument::parse(xmlString);
		if (!xml) {
			SimpleLogger::instance()->postMessage("Settings file corrupt, error parsing audio setup");
		}
	}
	auto audioError = deviceManager_.initialise(1, 0, xml.get(), true);
	if (!audioError.isEmpty()) {
		SimpleLogger::instance()->postMessage("Error initializing audio device manager: " + audioError);
	}
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

	// Save the selected Audio device for the next startup
	auto xml = deviceManager_.createStateXml();
	if (xml) {
		// Could be nullptr if the user did not touch the default...
		String setupString = xml->toString();
		// Store in the settings file
		Settings::instance().set("audioSetup", setupString.toStdString());
	}
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

File getOrCreateSubdirectory(File dir, String const &name) {
	File subdir = dir.getChildFile(name);
	if (!subdir.exists()) {
		subdir.createDirectory();
	}
	return subdir;
}

File RecordingView::getPrehearDirectory() {
	auto knobkraftorm = getOrCreateSubdirectory(File::getSpecialLocation(File::userApplicationDataDirectory), "KnobKraftOrm"); //TODO this should be a define?
	return getOrCreateSubdirectory(knobkraftorm, "PatchPrehear");
}

void RecordingView::sampleNote() {
	if (!UIModel::currentPatch().patch()) return;

	auto patchMD5 = midikraft::PatchHolder::calcMd5(UIModel::currentSynth(), UIModel::currentPatch().patch());

	File directory = getPrehearDirectory();
	std::string filename = directory.getChildFile(patchMD5 + ".wav").getFullPathName().toStdString();

	// Ok, what we'll do is to 
	// a) start the recorder to listen for audio coming in
	// b) send a MIDI note to the current synth
	// register a callback that the recorder will call when the signal is done, and then refresh the Thumbnail
	recorder_.startRecording(filename ,true, [this]() {
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
