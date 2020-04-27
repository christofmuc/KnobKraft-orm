/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "KorgDW8000Patch.h"

#include "KorgDW8000Parameter.h"

#include <boost/format.hpp>

namespace midikraft {

	const int kKorgDW8000DataTypeID = 0; // The DW8000 has only one datatype, no layers, tones, tunings, or other stuff

	std::string KorgDW8000PatchNumber::friendlyName() const {
		return (boost::format("%d%d") % ((programNumber_.toZeroBased() / 8) + 1) % ((programNumber_.toZeroBased() % 8) + 1)).str();
	}

	KorgDW8000Patch::KorgDW8000Patch(Synth::PatchData const &patchdata, std::string const &name) : Patch(kKorgDW8000DataTypeID, patchdata), name_(name)
	{
		// The DW 8000 uses an astounding amount of 51 bytes per patch
		jassert(patchdata.size() == 51);
	}

	std::string KorgDW8000Patch::patchName() const
	{
		return name_;
	}

	void KorgDW8000Patch::setName(std::string const &name)
	{
		name_ = name;
	}

	std::shared_ptr<PatchNumber> KorgDW8000Patch::patchNumber() const {
		return std::make_shared<KorgDW8000PatchNumber>(number_);
	}

	void KorgDW8000Patch::setPatchNumber(MidiProgramNumber patchNumber) {
		number_ = KorgDW8000PatchNumber(patchNumber);
	}

	int KorgDW8000Patch::value(SynthParameterDefinition const &param) const
	{
		ignoreUnused(param);
		throw std::logic_error("The method or operation is not implemented.");
	}

	SynthParameterDefinition const & KorgDW8000Patch::paramBySysexIndex(int sysexIndex) const
	{
		ignoreUnused(sysexIndex);
		throw std::logic_error("The method or operation is not implemented.");
	}

	std::vector<std::shared_ptr<SynthParameterDefinition>> KorgDW8000Patch::allParameterDefinitions()
	{
		return KorgDW8000Parameter::allParameters;
	}

}
