/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "Patch.h"
#include "DetailedParametersCapability.h"
#include "StoredPatchNameCapability.h"

#include "Matrix1000ParamDefinition.h"

namespace midikraft {

	class Matrix1000Patch : public Patch, public StoredPatchNameCapability, public DefaultNameCapability, public DetailedParametersCapability {
	public:
		Matrix1000Patch(Synth::PatchData const &patchdata, MidiProgramNumber place);

		virtual std::string name() const override;
		virtual MidiProgramNumber patchNumber() const override;

		// StoredPatchNameCapability
		virtual void setName(std::string const &name) override;

		// DefaultNameCapability
		virtual bool isDefaultName(std::string const &patchName) const override;

		int value(SynthParameterDefinition const &param) const;
		int param(Matrix1000Param id) const;
		SynthParameterDefinition const &paramBySysexIndex(int sysexIndex) const;

		bool paramActive(Matrix1000Param id) const;
		std::string lookupValue(Matrix1000Param id) const;

		virtual std::vector<std::shared_ptr<SynthParameterDefinition>> allParameterDefinitions() const override;

	private:
		MidiProgramNumber number_;
	};

}


