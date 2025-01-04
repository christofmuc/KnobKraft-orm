#pragma once

#include "JuceHeader.h"

#include "MidiLogView.h"


class MidiLogPanel : public Component {
public:
	MidiLogPanel();
	virtual ~MidiLogPanel() override = default;

	virtual void resized() override;

	MidiLogView& log() { return midiLogView_; }

private:
	void send();
	void sendToMidiOuts(std::vector<MidiMessage> const& messages);

	Label sysexEntryLabel_;
	TextEditor sysexEntry_;
	TextButton sendSysex_;
	MidiLogView midiLogView_;
};