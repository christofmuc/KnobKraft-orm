#include "RefaceDXPatch.h"

#include <algorithm>
#include <boost/format.hpp>

RefaceDXPatch::RefaceDXPatch(Synth::PatchData const &voiceData) : Patch(voiceData), originalProgramNumber_(MidiProgramNumber::fromZeroBase(0))
{
}

std::string RefaceDXPatch::patchName() const
{
	std::string result;
	// Extract the first 10 bytes of the common block, that's the ascii name
	for (int i = 0; i < std::max((size_t) 10, data().size()); i++) {
		result.push_back(data()[i]);
	}
	return result;
}

void RefaceDXPatch::setName(std::string const &name)
{
	throw std::logic_error("The method or operation is not implemented.");
}

std::shared_ptr<PatchNumber> RefaceDXPatch::patchNumber() const
{
	return std::make_shared<RefaceDXPatchNumber>(originalProgramNumber_);
}

void RefaceDXPatch::setPatchNumber(MidiProgramNumber patchNumber)
{
	originalProgramNumber_ = patchNumber;
}

int RefaceDXPatch::value(SynthParameterDefinition const &param) const
{
	throw std::logic_error("The method or operation is not implemented.");
}

SynthParameterDefinition const & RefaceDXPatch::paramBySysexIndex(int sysexIndex) const
{
	throw std::logic_error("The method or operation is not implemented.");
}

std::vector<std::string> RefaceDXPatch::warnings()
{
	throw std::logic_error("The method or operation is not implemented.");
}

std::vector<SynthParameterDefinition *> RefaceDXPatch::allParameterDefinitions()
{
	return std::vector<SynthParameterDefinition *>();
}

std::string RefaceDXPatchNumber::friendlyName() const
{
	int bank = midiProgramNumber().toZeroBased() / 8;
	int patch = midiProgramNumber().toZeroBased() % 8;
	return (boost::format("Bank%d-%d") % bank % patch).str();
}
