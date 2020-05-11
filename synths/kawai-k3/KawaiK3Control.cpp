/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "KawaiK3Control.h"

namespace midikraft {

	juce::MidiMessage midikraft::KawaiK3Control::createSetParameterMessage(KawaiK3 &k3, KawaiK3Parameter *param, int paramValue)
	{
		//TODO - this contains the BCR specific offset, that should not be here!
		uint8 highNibble, lowNibble;
		if (param->minValue() < 0) {
			// For parameters with negative values, we have offset the values by -minValue(), so we need to add minValue() again
			int correctedValue = paramValue + param->minValue(); // minValue is negative, so this will subtract actually something resulting in a negative number...
			int clampedValue = std::min(std::max(correctedValue, param->minValue()), param->maxValue());

			// Now, the K3 unfortunately uses a sign bit for the negative values, which makes it completely impossible to be used directly with the BCR2000
			if (clampedValue < 0) {
				highNibble = (((-clampedValue) & 0xFF) | 0x80) >> 4;
				lowNibble = (-clampedValue) & 0x0F;
			}
			else {
				highNibble = (clampedValue & 0xF0) >> 4;
				lowNibble = (clampedValue & 0x0F);
			}
		}
		else {
			// Just clamp to the min max range
			int correctedValue = std::min(std::max(paramValue, param->minValue()), param->maxValue());
			highNibble = (correctedValue & 0xF0) >> 4;
			lowNibble = (correctedValue & 0x0F);
		}

		// Now build the sysex message (p. 48 of the K3 manual)
		auto dataBlock = k3.buildSysexFunction(KawaiK3::PARAMETER_SEND, (uint8)param->paramNo());
		dataBlock.push_back(highNibble);
		dataBlock.push_back(lowNibble);
		return MidiMessage::createSysExMessage(&dataBlock[0], static_cast<int>(dataBlock.size()));
	}

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
		// Ok, not too much to do here I hope
		if (ccMessage.isController()) {
			int value = ccMessage.getControllerValue(); // This will give us 0..127 at most. I assume they have been configured properly for the K3
			int controller = ccMessage.getControllerNumber();
			if (controller >= 1 && controller <= 39) {
				// Ok, this is within the proper range of the Kawai K3 controllers
				auto param = KawaiK3Parameter::findParameter((KawaiK3Parameter::Parameter) controller);
				if (param != nullptr) {
					return createSetParameterMessage(k3, param, value);
				}
			}
		}
		return ccMessage;
	}

}
