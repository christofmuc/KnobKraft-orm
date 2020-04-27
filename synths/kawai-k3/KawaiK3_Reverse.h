#pragma once

#include "KawaiK3.h"

#include "EditBufferHandler.h"

class KawaiK3_Reverse {
public:
	KawaiK3_Reverse(KawaiK3 &k3);

	// For reverse engineering the sysex format
	void createReverseEngineeringData(MidiOutput *midiOutput, EditBufferHandler &continuationHandler, SimpleLogger &logger);

private:
	KawaiK3 &k3_;
	EditBufferHandler::HandlerHandle handle_ = EditBufferHandler::makeNone();
	EditBufferHandler *handler_;

	juce::MidiMessage emptyTone();
	void handleNextEditBufferDump(MidiOutput *midiOutput, juce::MidiMessage editBuffer, SimpleLogger &logger);
};


