/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "JuceHeader.h"

#include "KawaiK3.h"
#include "KawaiK3Parameter.h"

namespace midikraft {

	class KawaiK3Control {
	public:
		juce::MidiMessage createSetParameterMessage(KawaiK3 &k3, KawaiK3Parameter *param, int paramValue);
		void determineParameterChangeFromSysex(KawaiK3 &k3, juce::MidiMessage const &message, KawaiK3Parameter **param, int *paramValue);
		juce::MidiMessage mapCCtoSysex(KawaiK3 &k3, juce::MidiMessage const &ccMessage);
	};

}

