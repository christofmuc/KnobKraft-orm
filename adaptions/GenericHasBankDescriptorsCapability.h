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
        virtual ~GenericHasBankDescriptorsCapability() = default;
		virtual std::vector<midikraft::BankDescriptor> bankDescriptors() const override;
		virtual std::vector<juce::MidiMessage> bankSelectMessages(MidiBankNumber bankNo) const override;

	private:
		GenericAdaptation* me_;
	};


}
