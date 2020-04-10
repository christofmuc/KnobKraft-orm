#pragma once

#include "Patch.h"

#include "Matrix1000ParamDefinition.h"

class Matrix1000PatchNumber : public PatchNumber {
public:
	using PatchNumber::PatchNumber;
	virtual std::string friendlyName() const override;
};

class Matrix1000Patch : public Patch {
public:
	Matrix1000Patch(Synth::PatchData const &patchdata);

	virtual std::string patchName() const override;
	virtual void setName(std::string const &name) override;
	virtual std::shared_ptr<PatchNumber> patchNumber() const override;
	virtual void setPatchNumber(MidiProgramNumber patchNumber) override;
	virtual int value(SynthParameterDefinition const &param) const;
	virtual int param(Matrix1000Param id) const;
	virtual SynthParameterDefinition const &paramBySysexIndex(int sysexIndex) const;
	virtual std::vector<std::string> warnings() override;

	bool paramActive(Matrix1000Param id) const;
	std::string lookupValue(Matrix1000Param id) const;

	virtual std::vector<SynthParameterDefinition *> allParameterDefinitions() override;

private:
	Matrix1000PatchNumber number_;
};



