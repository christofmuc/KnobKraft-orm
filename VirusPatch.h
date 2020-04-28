/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "Patch.h"
#include "PatchNumber.h"

namespace midikraft {

	class VirusPatchNumber : public PatchNumber {
	public:
		using PatchNumber::PatchNumber;
		virtual std::string friendlyName() const override;
	};

	class VirusPatch : public Patch, public StoredPatchNameCapability {
	public:
		VirusPatch(Synth::PatchData const &data);

		virtual std::string name() const override;
		virtual std::shared_ptr<PatchNumber> patchNumber() const override;
		virtual void setPatchNumber(MidiProgramNumber patchNumber) override;
		
		virtual std::vector<std::shared_ptr<SynthParameterDefinition>> allParameterDefinitions() override;

		// StoredPatchNameCapability
		virtual void setName(std::string const &name) override;

		enum DataFileTypes { PATCH_VIRUS_B = 0 };
		enum Page { PageA = 0, PageB = 1 };
		static int index(Page page, int index);

	private:
		MidiProgramNumber place_ = MidiProgramNumber::fromZeroBase(0);
	};

}
