/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "Patch.h"
#include "DetailedParametersCapability.h"

namespace midikraft {

	class KorgDW8000Patch : public Patch, public DetailedParametersCapability {
	public:
		KorgDW8000Patch(Synth::PatchData const &patchdata, MidiProgramNumber const &programNumber);

		virtual std::string name() const override;
		virtual MidiProgramNumber patchNumber() const override;

		virtual std::vector<std::shared_ptr<SynthParameterDefinition>> allParameterDefinitions() const override;

	private:
		MidiProgramNumber number_;
	};

}
