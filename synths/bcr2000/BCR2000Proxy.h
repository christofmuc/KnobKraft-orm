/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "SynthParameterDefinition.h"

namespace midikraft {

	class BCR2000Proxy {
	public:
		virtual void setRotaryParam(int knobNumber, SynthParameterDefinition *param) = 0;
		virtual void setRotaryParamValue(int encoder, int value) = 0;
		virtual void setButtonParam(int knobNumber, std::string const &name) = 0;
	};

}
