/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "Patch.h"
#include "StoredPatchNameCapability.h"

namespace midikraft {

	class OB6Patch : public Patch, public StoredPatchNameCapability, public DefaultNameCapability {
	public:
		OB6Patch(int dataTypeID, Synth::PatchData const &patchData, MidiProgramNumber programNo);

		virtual std::string name() const override;
		
		virtual MidiProgramNumber patchNumber() const override;

		// StoredPatchNameCapability - even if the OB6 does not show patch names, it stores them!
		virtual void setName(std::string const &name) override;
		virtual bool isDefaultName(std::string const &patchName) const override;

	private:
		MidiProgramNumber place_;
	};

}
