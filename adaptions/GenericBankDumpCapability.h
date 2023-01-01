/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "BankDumpCapability.h"

#include "GenericAdaptation.h"

namespace knobkraft {

	class GenericBankDumpCapability : public midikraft::BankDumpCapability {
	public:
		GenericBankDumpCapability(GenericAdaptation *me) : me_(me) {}
        virtual ~GenericBankDumpCapability() = default;
		std::vector<MidiMessage> requestBankDump(MidiBankNumber bankNo) const override;
		bool isBankDump(const MidiMessage& message) const override;
		bool isBankDumpFinished(std::vector<MidiMessage> const &bankDump) const override;
		midikraft::TPatchVector patchesFromSysexBank(const MidiMessage& message) const override;

	private:
		GenericAdaptation *me_;
	};

}
