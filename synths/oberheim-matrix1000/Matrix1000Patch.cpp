#include "Matrix1000Patch.h"

#include "Matrix1000ParamDefinition.h"

#include <boost/format.hpp>

std::string Matrix1000PatchNumber::friendlyName() const {
	// The Matrix does a 3 digit display, with the first patch being "000" and the highest being "999"s 
	return (boost::format("%03d") % programNumber_.toZeroBased()).str();
}

Matrix1000Patch::Matrix1000Patch(Synth::PatchData const &patchdata) : Patch(patchdata)
{
}

std::string Matrix1000Patch::patchName() const
{
	// The patch name are the first 8 bytes ASCII
	std::string name;
	for (int i = 0; i < 8; i++) {
		uint8 charValue = at(i);
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
	if (param.valueInPatch(*this, result)) {
		return result;
	}
	throw new std::runtime_error("Invalid parameter");
}

int Matrix1000Patch::param(Matrix1000Param id) const
{
	auto &param = Matrix1000ParamDefinition::param(id);
	int value;
	if (param.valueInPatch(*this, value)) {
		return value;
	}
	throw new std::runtime_error("Missing parameter value");
}

SynthParameterDefinition const & Matrix1000Patch::paramBySysexIndex(int sysexIndex) const
{
	for (auto param : Matrix1000ParamDefinition::allDefinitions) {
		if (param->sysexIndex() == sysexIndex) {
			//! TODO- this is a bad way to address the parameters, as this is not uniquely defined
			return *param;
		}
	}
	throw new std::runtime_error("Bogus call");
}

bool Matrix1000Patch::paramActive(Matrix1000Param id) const
{
	auto &param = Matrix1000ParamDefinition::param(id);
	return param.isActive(this);
}

std::string Matrix1000Patch::lookupValue(Matrix1000Param id) const
{
	auto &param = Matrix1000ParamDefinition::param(id);
	int value;
	if (param.valueInPatch(*this, value)) {
		return param.valueAsText(value);
	}
	else {
		return "unknown";
	}
}

std::vector<SynthParameterDefinition *> Matrix1000Patch::allParameterDefinitions()
{
	return Matrix1000ParamDefinition::allDefinitions;
}

std::vector<std::string> Matrix1000Patch::warnings()
{
	throw std::logic_error("The method or operation is not implemented.");
}
