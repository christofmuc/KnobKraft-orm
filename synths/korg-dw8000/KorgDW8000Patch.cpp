/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "KorgDW8000Patch.h"

#include "KorgDW8000Parameter.h"

#include <boost/format.hpp>

namespace midikraft {

	const int kKorgDW8000DataTypeID = 0; // The DW8000 has only one data-type, no layers, tones, tunings, or other stuff

	std::string KorgDW8000PatchNumber::friendlyName() const {
		return (boost::format("%d%d") % ((programNumber_.toZeroBased() / 8) + 1) % ((programNumber_.toZeroBased() % 8) + 1)).str();
	}

	KorgDW8000Patch::KorgDW8000Patch(Synth::PatchData const &patchdata, MidiProgramNumber const &programNumber) : Patch(kKorgDW8000DataTypeID, patchdata), number_(programNumber)
	{
		// The DW 8000 uses an astounding amount of 51 bytes per patch
		jassert(patchdata.size() == 51);
	}

	std::string KorgDW8000Patch::name() const
	{
		return number_.friendlyName();
	}

	std::shared_ptr<PatchNumber> KorgDW8000Patch::patchNumber() const {
		return std::make_shared<KorgDW8000PatchNumber>(number_);
	}

	void KorgDW8000Patch::setPatchNumber(MidiProgramNumber patchNumber) {
		number_ = KorgDW8000PatchNumber(patchNumber);
	}

	std::vector<std::shared_ptr<SynthParameterDefinition>> KorgDW8000Patch::allParameterDefinitions() const
	{
		return KorgDW8000Parameter::allParameters;
	}

}
