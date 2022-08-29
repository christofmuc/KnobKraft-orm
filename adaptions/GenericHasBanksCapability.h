/*
   Copyright (c) 2022 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "JuceHeader.h"

#include "HasBanksCapability.h"

namespace knobkraft {

	class GenericAdaptation;

	class GenericHasBanksCapability : public midikraft::HasBanksCapability
	{
	public:
		GenericHasBanksCapability(GenericAdaptation* me) : me_(me) {}

		int numberOfBanks() const override;
		int numberOfPatches() const override;
		std::string friendlyBankName(MidiBankNumber bankNo) const override;

	private:
		GenericAdaptation* me_;
	};



}
