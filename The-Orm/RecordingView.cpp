/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "RecordingView.h"

#include "Logger.h"
#include "MidiNote.h"
#include "UIModel.h"
#include "Settings.h"
#include "AutoThumbnailingDialog.h"

RecordingView::RecordingView(PatchView &patchView) : patchView_(patchView), deviceSelector_(deviceManager_, 1, 2, 1, 1, false, false, true, false),
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
	{ "autoThumbnail", { 1, "Create thumbnails", [this]() {
		AutoThumbnailingDialog dialog(patchView_, *this);
		dialog.runThread();
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

void RecordingView::sampleNote() {
	if (!UIModel::currentPatch().patch()) return;

	auto patchMD5 = UIModel::currentPatch().md5();

	std::string filename = UIModel::getPrehearDirectory().getChildFile(patchMD5 + ".wav").getFullPathName().toStdString();

	// Ok, what we'll do is to 
	// a) start the recorder to listen for audio coming in
	// b) send a MIDI note to the current synth
	// register a callback that the recorder will call when the signal is done, and then refresh the Thumbnail
	recorder_.startRecording(filename ,true, [this]() {
		String filename = recorder_.getFilename();
		thumbnail_.loadFromFile(filename.toStdString(), "");
		UIModel::instance()->thumbnails_.sendChangeMessage();
	});

	auto device = std::dynamic_pointer_cast<midikraft::DiscoverableDevice>(UIModel::instance()->currentSynth_.smartSynth());
	if (device->wasDetected()) {
		auto location = midikraft::Capability::hasCapability<midikraft::MidiLocationCapability>(UIModel::instance()->currentSynth_.smartSynth());
		auto noteNumber = MidiNote(440.0).noteNumber();
		if (location) {
			auto noteOn = MidiMessage::noteOn(location->channel().toOneBasedInt(), noteNumber, (uint8)127);
			auto noteOff = MidiMessage::noteOff(location->channel().toOneBasedInt(), noteNumber);
			midiSender_.addMessageToBuffer(location->midiOutput(), noteOn, 0.0);
			midiSender_.addMessageToBuffer(location->midiOutput(), noteOff, 0.5);
		}
	}
}

bool RecordingView::hasDetectedSignal() const
{
	return recorder_.hasDetectedSignal();
}

void RecordingView::changeListenerCallback(ChangeBroadcaster* source)
{
	ignoreUnused(source);
	thumbnail_.repaint();
}
