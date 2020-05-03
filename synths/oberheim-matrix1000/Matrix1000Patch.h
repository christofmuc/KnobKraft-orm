/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "Patch.h"
#include "DetailedParametersCapability.h"

#include "Matrix1000ParamDefinition.h"

namespace midikraft {

	class Matrix1000PatchNumber : public PatchNumber {
	public:
		using PatchNumber::PatchNumber;
		virtual std::string friendlyName() const override;
	};

	class Matrix1000Patch : public Patch, public StoredPatchNameCapability, public DetailedParametersCapability {
	public:
		Matrix1000Patch(Synth::PatchData const &patchdata);

		virtual std::string name() const override;
		virtual void setName(std::string const &name) override;
		virtual bool isDefaultName() const override;
		virtual std::shared_ptr<PatchNumber> patchNumber() const override;
		virtual void setPatchNumber(MidiProgramNumber patchNumber) override;

		int value(SynthParameterDefinition const &param) const;
		int param(Matrix1000Param id) const;
		SynthParameterDefinition const &paramBySysexIndex(int sysexIndex) const;

		bool paramActive(Matrix1000Param id) const;
		std::string lookupValue(Matrix1000Param id) const;

		virtual std::vector<std::shared_ptr<SynthParameterDefinition>> allParameterDefinitions() override;

	private:
		Matrix1000PatchNumber number_;
	};

}


