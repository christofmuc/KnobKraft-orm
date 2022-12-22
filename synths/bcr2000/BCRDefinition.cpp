/*
   Copyright (c) 2019 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "BCRDefinition.h"

#include <map>

namespace midikraft {

	std::map<BCRledMode, std::string> ledModeMap = {
		{ OFF, "off" },
		{ ONE_DOT, "1dot" },
		{ ONE_DOT_OFF, "1dot/off" },
		{ ONETWO_DOT, "12dot" },
		{ ONETWO_DOT_OFF, "12dot/off" },
		{ BAR, "bar" },
		{ BAR_OFF, "bar/off" },
		{ SPREAD, "spread" },
		{ PAN, "pan" },
		{ QUAL, "qual" },
		{ CUT, "cut" },
		{ DAMP, "damp" }
	};

	BCRdefinition::BCRdefinition(BCRtype type, int encoderNumber) : type_(type), number_(encoderNumber)
	{
	}

	std::string BCRdefinition::ledMode(BCRledMode ledmode)
	{
		return ledModeMap[ledmode];
	}

}
