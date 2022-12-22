/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "KorgDW8000Patch.h"

#include "KorgDW8000Parameter.h"
#include "KorgDW8000.h"

#include <boost/format.hpp>

namespace midikraft {

	const int kKorgDW8000DataTypeID = 0; // The DW8000 has only one data-type, no layers, tones, tunings, or other stuff

	KorgDW8000Patch::KorgDW8000Patch(Synth::PatchData const &patchdata, MidiProgramNumber const &programNumber) : Patch(kKorgDW8000DataTypeID, patchdata), number_(programNumber)
	{
		// The DW 8000 uses an astounding amount of 51 bytes per patch
		jassert(patchdata.size() == 51);
	}

	std::string KorgDW8000Patch::name() const
	{
		KorgDW8000 dw;
		return dw.friendlyProgramName(number_); //TODO Could call a static method here
	}

	MidiProgramNumber KorgDW8000Patch::patchNumber() const {
		return number_;
	}

	std::vector<std::shared_ptr<SynthParameterDefinition>> KorgDW8000Patch::allParameterDefinitions() const
	{
		return KorgDW8000Parameter::allParameters;
	}

}
