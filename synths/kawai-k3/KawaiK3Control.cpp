/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "KawaiK3Control.h"

namespace midikraft {

	void midikraft::KawaiK3Control::determineParameterChangeFromSysex(KawaiK3 &k3, juce::MidiMessage const &message, KawaiK3Parameter **param, int *paramValue)
	{
		if (k3.isOwnSysex(message)) {
			if (k3.sysexFunction(message) == KawaiK3::PARAMETER_SEND) {
				// Yep, that's us. Find the parameter definition and calculate the new value of that parameter
				auto paramNo = k3.sysexSubcommand(message);
				if (paramNo >= 1 && paramNo <= 39) {
					auto paramFound = KawaiK3Parameter::findParameter(static_cast<KawaiK3Parameter::Parameter>(paramNo));
					if (paramFound) {
						if (message.getSysExDataSize() > 7) {
							uint8 highNibble = message.getSysExData()[6];
							uint8 lowNibble = message.getSysExData()[7];
							int value = (highNibble << 4) | lowNibble;
							if (paramFound->minValue() < 0) {
								// Special handling for sign bit in K3
								if ((value & 0x80) == 0x80) {
									value = -(value & 0x7f);
								}
							}

							// Only now we do set our output variables
							*param = paramFound;
							*paramValue = value;
						}
					}
				}
			}
		}
	}

	juce::MidiMessage midikraft::KawaiK3Control::mapCCtoSysex(KawaiK3 &k3, juce::MidiMessage const &ccMessage)
	{
		ignoreUnused(k3);
		// Ok, not too much to do here I hope
		if (ccMessage.isController()) {
			//int value = ccMessage.getControllerValue(); // This will give us 0..127 at most. I assume they have been configured properly for the K3
			int controller = ccMessage.getControllerNumber();
			if (controller >= 1 && controller <= 39) {
				// Ok, this is within the proper range of the Kawai K3 controllers
				auto param = KawaiK3Parameter::findParameter((KawaiK3Parameter::Parameter) controller);
				if (param != nullptr) {
					return {}; // createSetParameterMessage(k3, param, value);
				}
			}
		}
		return ccMessage;
	}

}
