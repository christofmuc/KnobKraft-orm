/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "KawaiK3Patch.h"

#include "Capability.h"

#include "KawaiK3.h"
#include "KawaiK3Wave.h"

#include <boost/format.hpp>

namespace midikraft {

	std::string KawaiK3PatchNumber::friendlyName() const {
		return (boost::format("%02d") % programNumber_.toOneBased()).str();
	}

	KawaiK3Patch::KawaiK3Patch(MidiProgramNumber programNo, Synth::PatchData const &patchdata) 
		: Patch(programNo.toZeroBased() >= 100 ? KawaiK3::K3_WAVE : KawaiK3::K3_PATCH, patchdata), number_(programNo)
	{
	}

	std::shared_ptr<KawaiK3Patch> KawaiK3Patch::createInitPatch()
	{
		std::vector<uint8> initPatchSyx(34);
		std::vector<std::pair<KawaiK3Parameter::Parameter, uint8>> initValues = {
		{ KawaiK3Parameter::OSC1_WAVE_SELECT, uint8(30) },
		{ KawaiK3Parameter::OSC2_WAVE_SELECT, uint8(30) },
		{ KawaiK3Parameter::PITCH_BEND, uint8(2) },
		{ KawaiK3Parameter::CUTOFF, uint8(99) },
		{ KawaiK3Parameter::VCA_LEVEL, uint8(31) },
		{ KawaiK3Parameter::VCA_SUSTAIN, uint8(31) },
		{ KawaiK3Parameter::LFO_SPEED, uint8(15) },
		{ KawaiK3Parameter::DEFAULT_PARAMETER, uint8(10) },
		};

		// Poke this into the sysex bytes
		auto patch = std::make_shared<KawaiK3Patch>(MidiProgramNumber::fromZeroBase(0), initPatchSyx); // The init patch always is patch #0
		for (auto value : initValues) {
			auto param = midikraft::Capability::hasCapability<SynthIntParameterCapability>(KawaiK3Parameter::findParameter(value.first));
			if (param) {
				param->setInPatch(*patch, value.second);
			}
		}

		return patch;
	}

	std::string KawaiK3Patch::name() const
	{
		// The Kawai K3 is so old is has no display to display a patch name, hence, also none stored in the patch data
		return number_.friendlyName();
	}

	std::shared_ptr<PatchNumber> KawaiK3Patch::patchNumber() const {
		return std::make_shared<KawaiK3PatchNumber>(number_);
	}

	void KawaiK3Patch::setPatchNumber(MidiProgramNumber patchNumber) {
		number_ = KawaiK3PatchNumber(patchNumber);
	}

	std::vector<std::shared_ptr<SynthParameterDefinition>> KawaiK3Patch::allParameterDefinitions() const
	{
		return KawaiK3Parameter::allParameters;
	}

	bool KawaiK3Patch::needsUserWave() const
	{
		// Determine if one of our two oscillators uses the "user" waveform!
		bool usesUserWave = false;
		auto wave1 = KawaiK3Parameter::findParameter(KawaiK3Parameter::OSC1_WAVE_SELECT);
		auto wave2 = KawaiK3Parameter::findParameter(KawaiK3Parameter::OSC2_WAVE_SELECT);
		if (wave1 && wave2) {
			int value;
			if (wave1->valueInPatch(*this, value)) {
				// 32 is the "programmable" wave form
				if (value == 32) usesUserWave = true;
			}
			if (wave2->valueInPatch(*this, value)) {
				// 32 is the "programmable" wave form
				if (value == 32) usesUserWave = true;
			}
		}
		else {
			jassertfalse;
		}
		return usesUserWave;
	}

	void KawaiK3Patch::addWaveIfOscillatorUsesIt(std::shared_ptr<KawaiK3Wave> wave)
	{
		jassert(wave);
		if (needsUserWave()) {
			// We want to append the user wave data to this patch's data, so the user wave is stored where it is needed
			if (wave) {
				auto patchData = data();
				std::copy(wave->data().cbegin(), wave->data().cend(), std::back_inserter(patchData));
				setData(patchData);
			}
				
		}
	}

}
