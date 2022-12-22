/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "BCRDefinition.h"

namespace midikraft {

	class CCBCRdefinition : public BCRStandardDefinition {
	public:
		CCBCRdefinition(int encoderNumber, int controllerNumber, int minValue, int maxValue, BCRledMode ledMode = ONE_DOT);
		CCBCRdefinition(BCRtype type, int encoderNumber, int controllerNumber, int minValue, int maxValue);
		virtual std::string generateBCR(int channel) const override;

	private:
		int controllerNumber_;
		int minValue_;
		int maxValue_;
		BCRledMode ledMode_;
	};

}
