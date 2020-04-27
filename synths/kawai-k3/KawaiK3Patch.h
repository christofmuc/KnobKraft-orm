/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "Patch.h"

#include "KawaiK3Parameter.h"
#include "PatchNumber.h"

namespace midikraft {

	class KawaiK3PatchNumber : public PatchNumber {
	public:
		using PatchNumber::PatchNumber;
		virtual std::string friendlyName() const;
	};

	class KawaiK3Patch : public Patch {
	public:
		KawaiK3Patch(MidiProgramNumber programNo, Synth::PatchData const &patchdata);

		static std::shared_ptr<KawaiK3Patch> createInitPatch();

		// Implementation of Patch interface
		virtual std::string patchName() const override;
		virtual void setName(std::string const &name) override;
		virtual std::shared_ptr<PatchNumber> patchNumber() const override;
		virtual void setPatchNumber(MidiProgramNumber patchNumber) override;

		int value(SynthParameterDefinition const &param) const;
		void setValue(KawaiK3Parameter const &param, int value);

		virtual std::vector<std::shared_ptr<SynthParameterDefinition>> allParameterDefinitions() override;

	private:
		void setNameFromPatchNumber();

		KawaiK3PatchNumber number_;
		std::string name_;
	};

}
