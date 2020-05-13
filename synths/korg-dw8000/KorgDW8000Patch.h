/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "Patch.h"
#include "DetailedParametersCapability.h"

namespace midikraft {

	class KorgDW8000PatchNumber : public PatchNumber {
	public:
		using PatchNumber::PatchNumber;
		virtual std::string friendlyName() const override;
	};

	class KorgDW8000Patch : public Patch, public DetailedParametersCapability {
	public:
		KorgDW8000Patch(Synth::PatchData const &patchdata, MidiProgramNumber const &programNumber);

		virtual std::string name() const override;
		virtual std::shared_ptr<PatchNumber> patchNumber() const override;
		virtual void setPatchNumber(MidiProgramNumber patchNumber) override;

		virtual std::vector<std::shared_ptr<SynthParameterDefinition>> allParameterDefinitions() const override;

	private:
		KorgDW8000PatchNumber number_;
	};

}
