/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "Patch.h"
#include "PatchNumber.h"

namespace midikraft {

	class RefaceDXPatchNumber : public PatchNumber {
		using PatchNumber::PatchNumber;
		virtual std::string friendlyName() const override;
	};

	class RefaceDXPatch : public Patch, public StoredPatchNameCapability {
	public:
		typedef struct { std::vector<uint8> common; std::vector<uint8> op[4]; } TVoiceData;

		RefaceDXPatch(Synth::PatchData const &voiceData);

		virtual std::string name() const override;
		virtual void setName(std::string const &name) override;
		virtual bool isDefaultName() const override;

		virtual std::shared_ptr<PatchNumber> patchNumber() const override;
		virtual void setPatchNumber(MidiProgramNumber patchNumber) override;

		virtual std::vector<std::shared_ptr<SynthParameterDefinition>> allParameterDefinitions() override;

	private:
		friend class RefaceDX;
		MidiProgramNumber originalProgramNumber_;
	};

}
