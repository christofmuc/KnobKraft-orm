/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "Patch.h"
#include "PatchNumber.h"
#include "StoredPatchNameCapability.h"

namespace midikraft {

	class RefaceDXPatch : public Patch, public StoredPatchNameCapability {
	public:
		typedef struct { std::vector<uint8> common; std::vector<uint8> op[4]; int count;  } TVoiceData;

		RefaceDXPatch(Synth::PatchData const &voiceData, MidiProgramNumber place);

		virtual std::string name() const override;
		virtual void setName(std::string const &name) override;
		virtual bool isDefaultName() const override;

		virtual MidiProgramNumber patchNumber() const override;

	private:
		friend class RefaceDX;
		MidiProgramNumber originalProgramNumber_;
	};

}
