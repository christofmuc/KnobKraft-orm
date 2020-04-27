#pragma once

#include "Patch.h"
#include "PatchNumber.h"

class RefaceDXPatchNumber : public PatchNumber {
	using PatchNumber::PatchNumber;
	virtual std::string friendlyName() const override;
};

class RefaceDXPatch : public Patch {
public:
	typedef struct { std::vector<uint8> common; std::vector<uint8> op[4]; } TVoiceData;

	RefaceDXPatch(Synth::PatchData const &voiceData);

	virtual std::string patchName() const override;
	virtual void setName(std::string const &name) override;

	virtual std::shared_ptr<PatchNumber> patchNumber() const override;
	virtual void setPatchNumber(MidiProgramNumber patchNumber) override;

	virtual int value(SynthParameterDefinition const &param) const override;
	virtual SynthParameterDefinition const & paramBySysexIndex(int sysexIndex) const override;
	virtual std::vector<std::string> warnings() override;

	virtual std::vector<SynthParameterDefinition *> allParameterDefinitions() override;

private:
	friend class RefaceDX;
	MidiProgramNumber originalProgramNumber_;
};
