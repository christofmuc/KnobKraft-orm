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
		
		bool isBankDump(const MidiMessage& message) const override;
		bool isBankDumpFinished(std::vector<MidiMessage> const &bankDump) const override;
		midikraft::BankDumpCapability::HandshakeReply isMessagePartOfBankDump(const MidiMessage& message) const override;
		midikraft::BankDumpCapability::FinishedReply bankDumpFinishedWithReply(std::vector<MidiMessage> const &bankDump) const override;
		midikraft::TPatchVector patchesFromSysexBank(std::vector<MidiMessage> const& messages) const override;

	private:
		GenericAdaptation *me_;
	};

	class GenericBankDumpRequestCapability : public midikraft::BankDumpRequestCapability {
	public:
		GenericBankDumpRequestCapability(GenericAdaptation* me) : me_(me) {}
		virtual ~GenericBankDumpRequestCapability() = default;

		std::vector<MidiMessage> requestBankDump(MidiBankNumber bankNo) const override;

	private:
		GenericAdaptation* me_;
	};

	class GenericBankDumpSendCapability : public midikraft::BankSendCapability {
	public:
		GenericBankDumpSendCapability(GenericAdaptation* me) : me_(me) {}
		virtual ~GenericBankDumpSendCapability() = default;

		virtual std::vector<MidiMessage> createBankMessages(std::vector<std::vector<MidiMessage>> patches) override;

	private:
		GenericAdaptation* me_;
	};

}
