/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "Patch.h"

class KorgDW8000PatchNumber : public PatchNumber {
public:
	using PatchNumber::PatchNumber;
	virtual std::string friendlyName() const override;
};

class KorgDW8000Patch : public Patch {
public:
	KorgDW8000Patch(Synth::PatchData const &patchdata, std::string const &name);

	virtual std::string patchName() const override;
	virtual void setName(std::string const &name) override;
	virtual std::shared_ptr<PatchNumber> patchNumber() const override;
	virtual void setPatchNumber(MidiProgramNumber patchNumber) override;
	virtual int value(SynthParameterDefinition const &param) const override;
	virtual SynthParameterDefinition const & paramBySysexIndex(int sysexIndex) const override;
	virtual std::vector<std::string> warnings() override;
	virtual std::vector<SynthParameterDefinition *> allParameterDefinitions() override;

private:
	KorgDW8000PatchNumber number_;
	std::string name_;
};

