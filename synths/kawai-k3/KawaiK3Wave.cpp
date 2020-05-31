/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "KawaiK3Wave.h"

#include "KawaiK3.h"
#include "KawaiK3WaveParameter.h"

namespace midikraft {

	KawaiK3Wave::KawaiK3Wave(Synth::PatchData const& data, MidiProgramNumber programNo) : Patch(KawaiK3::K3_WAVE, data), programNo_(programNo)
	{
	}

	KawaiK3Wave::KawaiK3Wave(const Additive::Harmonics& harmonics, MidiProgramNumber programNo) : Patch(KawaiK3::K3_WAVE), programNo_(programNo)
	{
		KawaiK3HarmonicsParameters::fromHarmonics(harmonics, *this);
	}

	std::string KawaiK3Wave::name() const
	{
		return "User Wave";
	}

	std::shared_ptr<midikraft::PatchNumber> KawaiK3Wave::patchNumber() const
	{
		return std::make_shared <KawaiK3WaveNumber>(programNo_);
	}

	void KawaiK3Wave::setPatchNumber(MidiProgramNumber patchNumber)
	{
		programNo_ = patchNumber;
	}

	std::string KawaiK3WaveNumber::friendlyName() const
	{
		switch (programNumber_.toZeroBased()) {
		case 100: return "Internal";
		case 101: return "Cartridge";
		default: return "Invalid";
		}
	}

}