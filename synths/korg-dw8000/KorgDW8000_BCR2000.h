/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "BCRDefinition.h"
#include "KorgDW8000Parameter.h"

namespace midikraft {

	class KorgDW80002BCR2000Definition : public BCRStandardDefinition {
	public:
		KorgDW80002BCR2000Definition(BCRtype type, int number, KorgDW8000Parameter::Parameter param, BCRledMode ledMode = ONE_DOT) :
			BCRStandardDefinition(type, number), param_(param), ledMode_(ledMode) {}

		virtual std::string generateBCR(int channel) const;

	protected:
		KorgDW8000Parameter::Parameter param_;
		BCRledMode ledMode_;
	};


	class KorgDW8000BCR2000 {
	public:
		static std::string generateBCL(int channel);
	};

}
