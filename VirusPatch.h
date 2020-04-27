#pragma once

#include "Patch.h"
#include "PatchNumber.h"

class VirusPatchNumber : public PatchNumber {
public:
	using PatchNumber::PatchNumber;
	virtual std::string friendlyName() const override;
};

class VirusPatch : public Patch {
public:
	VirusPatch(Synth::PatchData const &data);

	virtual std::string patchName() const override;
	virtual void setName(std::string const &name) override;
	virtual std::shared_ptr<PatchNumber> patchNumber() const override;
	virtual void setPatchNumber(MidiProgramNumber patchNumber) override;
	virtual int value(SynthParameterDefinition const &param) const override;
	virtual SynthParameterDefinition const & paramBySysexIndex(int sysexIndex) const override;
	virtual std::vector<std::string> warnings() override;
	virtual std::vector<SynthParameterDefinition *> allParameterDefinitions() override;

	enum Page { PageA = 0, PageB = 1 };
	static int index(Page page, int index);

private:
	MidiProgramNumber place_ = MidiProgramNumber::fromZeroBase(0);
};
