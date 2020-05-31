/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "Synth.h"
#include "Patch.h"
#include "Additive.h"
#include "MidiProgramNumber.h"

namespace midikraft {

	class KawaiK3WaveNumber : public PatchNumber {
	public:
		using PatchNumber::PatchNumber;
		virtual std::string friendlyName() const;
	};

	//! This is the storage class for a stand-alone user wave of the Kawai K3. 
	class KawaiK3Wave : public Patch {
	public:
		KawaiK3Wave(Synth::PatchData const& data, MidiProgramNumber programNo);
		KawaiK3Wave(const Additive::Harmonics& harmonics, MidiProgramNumber programNo);

		std::string name() const override;

		std::shared_ptr<PatchNumber> patchNumber() const override;
		void setPatchNumber(MidiProgramNumber patchNumber) override;

	private:
		MidiProgramNumber programNo_;
	};

}



