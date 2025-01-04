/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "KawaiK3Wave.h"

#include "KawaiK3.h"
#include "KawaiK3WaveParameter.h"

namespace midikraft {

	KawaiK3Wave::KawaiK3Wave(Synth::PatchData const& data, MidiProgramNumber place) : DataFile(KawaiK3::K3_WAVE, data), programNo_(place)
	{
		jassert(data.size() == 64);
	}

	KawaiK3Wave::KawaiK3Wave(const Additive::Harmonics& harmonics, MidiProgramNumber place) : DataFile(KawaiK3::K3_WAVE), programNo_(place)
	{
		KawaiK3HarmonicsParameters::fromHarmonics(harmonics, *this);
	}

}