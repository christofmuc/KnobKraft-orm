/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "CCBCRDefinition.h"

#include "JuceHeader.h"

#include <fmt/format.h>

namespace midikraft {

	CCBCRdefinition::CCBCRdefinition(int encoderNumber, int controllerNumber, int minValue, int maxValue, BCRledMode ledMode) :
		BCRStandardDefinition(ENCODER, encoderNumber), controllerNumber_(controllerNumber), minValue_(minValue), maxValue_(maxValue), ledMode_(ledMode)
	{
	}

	CCBCRdefinition::CCBCRdefinition(BCRtype type, int encoderNumber, int controllerNumber, int minValue, int maxValue) :
		BCRStandardDefinition(type, encoderNumber), controllerNumber_(controllerNumber), minValue_(minValue), maxValue_(maxValue)
	{
	}

	std::string CCBCRdefinition::generateBCR(int channel) const
	{
		switch (type()) {
		case ENCODER:
			return fmt::format(
				"$encoder {} ; Standard CC Controller ${:02x}\n"
				"  .easypar CC {} {} {} {} absolute\n"
				"  .default 0\n"
				"  .mode {}\n"
				"  .showvalue on\n"
				"  .resolution 64 64 127 127\n",
			 number_, controllerNumber_, (channel + 1), controllerNumber_, minValue_, maxValue_, BCRdefinition::ledMode(ledMode_));
		case BUTTON: {
			std::string mode = "incval 1";
			std::string ccmode = "increment";
			if (maxValue_ == 1) {
				// If this is only a 0/1 button, we can put it to toggle so it lights up when the value is one
				mode = "toggle";
				ccmode = "toggleon";
			}
			return fmt::format(
				"$button {} ; Standard CC Controller ${:02x}\n"
				"  .easypar CC {} {} {} {} {}\n"
				"  .default 0\n"
				"  .mode {}\n"
				"  .showvalue on\n",
			 number_, controllerNumber_, (channel + 1), controllerNumber_, maxValue_, minValue_, ccmode, mode);
		}
		default:
			jassert(false);
			return "; Not supported\n";
		}
	}

}
