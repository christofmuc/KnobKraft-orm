/*
   Copyright (c) 2025 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "UserBankFactory.h"

#include "JuceHeader.h"

namespace knobkraft {

std::shared_ptr<midikraft::UserBank> createUserBank(std::shared_ptr<midikraft::Synth> synth,
	int bankSelected,
	std::string const& name,
	std::string const& id)
{
	auto bank = MidiBankNumber::fromZeroBase(bankSelected, midikraft::SynthBank::numberOfPatchesInBank(synth, bankSelected));
	std::string finalId = id.empty() ? Uuid().toString().toStdString() : id;
	return std::make_shared<midikraft::UserBank>(finalId, name, synth, bank);
}

} // namespace knobkraft
