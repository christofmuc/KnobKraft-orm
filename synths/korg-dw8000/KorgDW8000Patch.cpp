#include "KorgDW8000Patch.h"

#include "KorgDW8000Parameter.h"

#include <boost/format.hpp>

std::string KorgDW8000PatchNumber::friendlyName() const {
	return (boost::format("%d%d") % ((programNumber_.toZeroBased() / 8) + 1) % ((programNumber_.toZeroBased() % 8) + 1)).str();
}

KorgDW8000Patch::KorgDW8000Patch(Synth::PatchData const &patchdata, std::string const &name) : Patch(patchdata), name_(name)
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
	throw std::logic_error("The method or operation is not implemented.");
}

SynthParameterDefinition const & KorgDW8000Patch::paramBySysexIndex(int sysexIndex) const
{
	throw std::logic_error("The method or operation is not implemented.");
}

std::vector<std::string> KorgDW8000Patch::warnings()
{
	throw std::logic_error("The method or operation is not implemented.");
}

std::vector<SynthParameterDefinition *> KorgDW8000Patch::allParameterDefinitions()
{
	return KorgDW8000Parameter::allParameters;
}
