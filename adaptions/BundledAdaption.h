/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "JuceHeader.h"

namespace knobkraft {

	struct BundledAdaption {
		std::string synthName;
		std::string pythonModuleName;
		std::string adaptionSourceCode;
	};

	extern std::vector<BundledAdaption> gBundledAdaptions();

}

