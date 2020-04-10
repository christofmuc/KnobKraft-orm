/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "Matrix1000Patch.h"

#include "Matrix1000ParamDefinition.h"

#include <boost/format.hpp>

namespace midikraft {

	const int kMatrix1000DataType = 0; // The Matrix1000 has just one data file type - the patch. No layers, voices, alternate tunings...

	std::string Matrix1000PatchNumber::friendlyName() const {
		// The Matrix does a 3 digit display, with the first patch being "000" and the highest being "999"s 
		return (boost::format("%03d") % programNumber_.toZeroBased()).str();
	}

	Matrix1000Patch::Matrix1000Patch(Synth::PatchData const &patchdata) : Patch(kMatrix1000DataType, patchdata)
	{
	}

	std::string Matrix1000Patch::patchName() const
	{
		// The patch name are the first 8 bytes ASCII
		std::string name;
		for (int i = 0; i < 8; i++) {
			int charValue = at(i);
			if (charValue < 32) {
				//WTF? I found old factory banks that had the letters literally as the "Number of the letter in the Alphabet", 1-based
				charValue += 'A' - 1;
			}
			name.push_back((char)charValue);
		}
		return name;
	}

	void Matrix1000Patch::setName(std::string const &name)
	{
		//TODO to be implemented
		ignoreUnused(name);
		jassert(false);
	}

	std::shared_ptr<PatchNumber> Matrix1000Patch::patchNumber() const {
		return std::make_shared<Matrix1000PatchNumber>(number_);
	}

	void Matrix1000Patch::setPatchNumber(MidiProgramNumber patchNumber) {
		number_ = Matrix1000PatchNumber(patchNumber);
	}

	int Matrix1000Patch::value(SynthParameterDefinition const &param) const
	{
		int result;
		auto intDefinition = dynamic_cast<SynthIntParameterCapability const *>(&param);
		if (intDefinition && intDefinition->valueInPatch(*this, result)) {
			return result;
		}
		throw new std::runtime_error("Invalid parameter");
	}

	int Matrix1000Patch::param(Matrix1000Param id) const
	{
		auto &param = Matrix1000ParamDefinition::param(id);
		return value(param);
	}

	SynthParameterDefinition const & Matrix1000Patch::paramBySysexIndex(int sysexIndex) const 
	{
		for (auto param : Matrix1000ParamDefinition::allDefinitions) {
			auto intParam = std::dynamic_pointer_cast<SynthIntParameterCapability>(param);
			if (intParam && intParam->sysexIndex() == sysexIndex) {
				//! TODO- this is a bad way to address the parameters, as this is not uniquely defined
				return *param;
			}
		}
		throw new std::runtime_error("Bogus call");
	}

	bool Matrix1000Patch::paramActive(Matrix1000Param id) const
	{
		auto &param = Matrix1000ParamDefinition::param(id);
		auto activeDefinition = dynamic_cast<SynthParameterActiveDetectionCapability const *>(&param);
		return !activeDefinition || activeDefinition->isActive(this);
	}

	std::string Matrix1000Patch::lookupValue(Matrix1000Param id) const
	{
		auto &param = Matrix1000ParamDefinition::param(id);
		return param.valueInPatchToText(*this);
	}

	std::vector<std::shared_ptr<SynthParameterDefinition>> Matrix1000Patch::allParameterDefinitions()
	{
		return Matrix1000ParamDefinition::allDefinitions;
	}

}
