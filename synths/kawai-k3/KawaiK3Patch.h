/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "Patch.h"
#include "DetailedParametersCapability.h"

#include "KawaiK3Parameter.h"
#include "PatchNumber.h"

#include "Additive.h"


namespace midikraft {

	class KawaiK3Wave;

	class KawaiK3PatchNumber : public PatchNumber {
	public:
		using PatchNumber::PatchNumber;
		virtual std::string friendlyName() const;
	};

	class KawaiK3Patch : public Patch, public DetailedParametersCapability {
	public:
		KawaiK3Patch(MidiProgramNumber programNo, Synth::PatchData const &patchdata);

		// Implementation of Patch interface
		virtual std::string name() const override;
		virtual std::shared_ptr<PatchNumber> patchNumber() const override;
		virtual void setPatchNumber(MidiProgramNumber patchNumber) override;

		// DetailedParametersCapability
		virtual std::vector<std::shared_ptr<SynthParameterDefinition>> allParameterDefinitions() const override;

		// Kawai K3 specific
		static std::shared_ptr<KawaiK3Patch> createInitPatch();
		bool needsUserWave() const;
		void addWaveIfOscillatorUsesIt(std::shared_ptr<KawaiK3Wave> wave);

	private:
		KawaiK3PatchNumber number_;
	};

}
