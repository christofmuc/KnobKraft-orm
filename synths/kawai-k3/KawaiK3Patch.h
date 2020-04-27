#pragma once

#include "Patch.h"

#include "KawaiK3Parameter.h"
#include "PatchNumber.h"

class KawaiK3PatchNumber : public PatchNumber {
public:
	using PatchNumber::PatchNumber;
	virtual std::string friendlyName() const;
};

class KawaiK3Patch: public Patch {
public:
	KawaiK3Patch(MidiProgramNumber programNo, Synth::PatchData const &patchdata);

	static std::shared_ptr<KawaiK3Patch> createInitPatch();

	// Implementation of Patch interface
	virtual std::string patchName() const override;
	virtual void setName(std::string const &name) override;
	virtual std::shared_ptr<PatchNumber> patchNumber() const override;
	virtual void setPatchNumber(MidiProgramNumber patchNumber) override;
	virtual int value(SynthParameterDefinition const &param) const override;
	void setValue(KawaiK3Parameter const &param, int value);

	// Not implemented yet
	virtual SynthParameterDefinition const & paramBySysexIndex(int sysexIndex) const override;
	virtual std::vector<std::string> warnings() override;
	virtual std::vector<SynthParameterDefinition *> allParameterDefinitions() override;

private:
	void setNameFromPatchNumber();

	KawaiK3PatchNumber number_;
	std::string name_;
};

