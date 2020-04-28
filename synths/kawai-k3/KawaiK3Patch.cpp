/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "KawaiK3Patch.h"

#include <boost/format.hpp>

namespace midikraft {

	const int kKawaiK3PatchDataTypeID = 0;

	std::string KawaiK3PatchNumber::friendlyName() const {
		return (boost::format("%02d") % programNumber_.toOneBased()).str();
	}

	KawaiK3Patch::KawaiK3Patch(MidiProgramNumber programNo, Synth::PatchData const &patchdata) : Patch(kKawaiK3PatchDataTypeID, patchdata), number_(programNo)
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
			patch->setValue(*KawaiK3Parameter::findParameter(value.first), value.second);
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

	int KawaiK3Patch::value(SynthParameterDefinition const &param) const
	{
		auto k3 = dynamic_cast<KawaiK3Parameter const &>(param);
		int result = (at(k3.sysexIndex() - 1) >> k3.shift()) & k3.bitMask();
		if (k3.minValue() < 0) {
			// This has a sign bit in bit 8
			if (at(k3.sysexIndex() - 1) & 0x80) {
				result = -result;
			}
		}
		return result;
	}

	void KawaiK3Patch::setValue(KawaiK3Parameter const &param, int value)
	{
		//TODO - range checking for the parameter, we might specify values out of range?
		int currentValue = at(param.sysexIndex() - 1);
		if (param.minValue() < 0) {
			if (value < 0) {
				uint8 cleanValue = (uint8)(currentValue & (~param.shiftedBitMask()));
				uint8 setValue = (uint8)(((abs(value) & param.bitMask()) << param.shift()) | 0x80); // Additionally set the sign bit
				setAt(param.sysexIndex() - 1, cleanValue | setValue);
			}
			else {
				uint8 cleanValue = currentValue & (!param.bitMask()) & 0x7F;  // Additionally clear out the sign bit
				uint8 setValue = (uint8)((value & param.bitMask()) << param.shift());
				setAt(param.sysexIndex() - 1, cleanValue | setValue);
			}
		}
		else {
			jassert(value >= 0);
			uint8 cleanValue = (uint8)(currentValue & (~param.shiftedBitMask()));
			uint8 setValue = (uint8)((value & param.bitMask()) << param.shift());
			setAt(param.sysexIndex() - 1, cleanValue | setValue);
		}
	}

	std::vector<std::shared_ptr<SynthParameterDefinition>> KawaiK3Patch::allParameterDefinitions()
	{
		std::vector<std::shared_ptr<SynthParameterDefinition>> result;
		//TODO This loop should be necessary
		for (auto param : KawaiK3Parameter::allParameters) {
			result.push_back(std::make_shared<KawaiK3Parameter>(*param));
		}
		return result;
	}

}
