/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "Synth.h"
#include "Patch.h"
#include "Additive.h"
#include "MidiProgramNumber.h"

namespace midikraft {

	//! This is the storage class for a stand-alone user wave of the Kawai K3. 
	class KawaiK3Wave : public DataFile {
	public:
		KawaiK3Wave(Synth::PatchData const& data, MidiProgramNumber programNo);
		KawaiK3Wave(const Additive::Harmonics& harmonics, MidiProgramNumber programNo);

	private:
		MidiProgramNumber programNo_;
	};

}



