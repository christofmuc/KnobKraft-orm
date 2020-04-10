#pragma once

#include "Patch.h"

class MKS80_PatchNumber : public PatchNumber {
public:
	MKS80_PatchNumber(MidiProgramNumber patchnumber);
	MKS80_PatchNumber(int bank, int patch);

	virtual std::string friendlyName() const override;
};

class MKS80_Patch : public Patch {
public:
	enum class APR_Section {
		PATCH_UPPER = (0b00110000 | 0b00000001),
		PATCH_LOWER = (0b00110000 | 0b00000010),
		TONE_UPPER = (0b00100000 | 0b00000001),
		TONE_LOWER = (0b00100000 | 0b00000010),
	};

	MKS80_Patch(std::shared_ptr<MKS80_PatchNumber> patchNumber, std::map<APR_Section, std::vector<uint8>> const &data);
	MKS80_Patch(MidiProgramNumber patchNumber, Synth::PatchData const &data);

	virtual std::string patchName() const override;
	virtual void setName(std::string const &name) override;

	virtual std::shared_ptr<PatchNumber> patchNumber() const override;
	virtual void setPatchNumber(MidiProgramNumber patchNumber) override;

	virtual int value(SynthParameterDefinition const &param) const override;
	virtual SynthParameterDefinition const & paramBySysexIndex(int sysexIndex) const override;
	virtual std::vector<std::string> warnings() override;

	virtual std::vector<SynthParameterDefinition *> allParameterDefinitions() override;

	// Support APR format creation
	MidiMessage buildAPRMessage(APR_Section section, MidiChannel channel) const;

	//! For validating the enum class before casting
	static bool isValidAPRSection(int section);

	//! For the funky DAT format
	static std::vector<uint8> toneFromDat(std::vector<uint8> const &dat);
	static std::vector<uint8> patchesFromDat(std::vector<uint8> const &dat);

private:
	void copyDataSection(std::map<APR_Section, std::vector<uint8>> const &data, std::vector<uint8> &result, MKS80_Patch::APR_Section section, int expectedLength);

	std::shared_ptr<MKS80_PatchNumber> patchNumber_;
};

