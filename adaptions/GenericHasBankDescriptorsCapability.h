/*
   Copyright (c) 2022 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "JuceHeader.h"

#include "HasBanksCapability.h"

namespace knobkraft {

	class GenericAdaptation;

	class GenericHasBankDescriptorsCapability : public midikraft::HasBankDescriptorsCapability
	{
	public:
		GenericHasBankDescriptorsCapability(GenericAdaptation* me) : me_(me) {}

		std::vector<midikraft::BankDescriptor> bankDescriptors() const override;

	private:
		GenericAdaptation* me_;
	};


}
