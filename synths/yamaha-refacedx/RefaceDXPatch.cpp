/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "RefaceDXPatch.h"

#include <algorithm>
#include <boost/format.hpp>

namespace midikraft {

	const int kRefaceDXPatchType = 0;

	RefaceDXPatch::RefaceDXPatch(Synth::PatchData const &voiceData) : Patch(kRefaceDXPatchType, voiceData), originalProgramNumber_(MidiProgramNumber::fromZeroBase(0))
	{
	}

	std::string RefaceDXPatch::name() const
	{
		std::string result;
		// Extract the first 10 bytes of the common block, that's the ascii name
		for (int i = 0; i < std::max((size_t)10, data().size()); i++) {
			result.push_back(data()[i]);
		}
		return result;
	}

	void RefaceDXPatch::setName(std::string const &name)
	{
		ignoreUnused(name);
		throw std::logic_error("The method or operation is not implemented.");
	}

	bool RefaceDXPatch::isDefaultName() const
	{
		return name() == "Init Voice";
	}

	std::shared_ptr<PatchNumber> RefaceDXPatch::patchNumber() const
	{
		return std::make_shared<RefaceDXPatchNumber>(originalProgramNumber_);
	}

	void RefaceDXPatch::setPatchNumber(MidiProgramNumber patchNumber)
	{
		originalProgramNumber_ = patchNumber;
	}

	std::vector<std::shared_ptr<SynthParameterDefinition>> RefaceDXPatch::allParameterDefinitions()
	{
		return {};
	}

	std::string RefaceDXPatchNumber::friendlyName() const
	{
		int bank = midiProgramNumber().toZeroBased() / 8;
		int patch = midiProgramNumber().toZeroBased() % 8;
		return (boost::format("Bank%d-%d") % bank % patch).str();
	}

}
