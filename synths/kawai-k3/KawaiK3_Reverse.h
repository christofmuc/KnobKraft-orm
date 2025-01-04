/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "KawaiK3.h"

#include "MidiController.h"

namespace midikraft {

	class KawaiK3_Reverse {
	public:
		KawaiK3_Reverse(KawaiK3 &k3);

		// For reverse engineering the sysex format
		void createReverseEngineeringData(MidiOutput *midiOutput);

	private:
		KawaiK3 &k3_;
		MidiController::HandlerHandle handle_ = MidiController::makeNoneHandle();

		juce::MidiMessage emptyTone();
		void handleNextEditBufferDump(MidiOutput *midiOutput, juce::MidiMessage editBuffer);
	};

}
