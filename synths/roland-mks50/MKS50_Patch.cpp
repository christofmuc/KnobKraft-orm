/*
   Copyright (c) 2021 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "MKS50_Patch.h"

#include "MKS50_Parameter.h"
#include "PackedDataFormatInfo.h"

#include <boost/format.hpp>

namespace midikraft {

	// How nice of Roland to specify the character mapping!
	const std::string MKS50_Patch::kPatchNameChar = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 -";

	MKS50_Patch::MKS50_Patch(MidiProgramNumber programNumber, std::string const& patchName, Synth::PatchData const& patchData) :
		Patch(static_cast<int>(MKS50DataType::ALL), patchData), programNumber_(programNumber), patchName_(patchName)
	{
	}

	std::string MKS50_Patch::name() const
	{
		return patchName_;
	}

	MidiProgramNumber MKS50_Patch::patchNumber() const
	{
		return programNumber_;
	}

	std::vector<std::shared_ptr<SynthParameterDefinition>> MKS50_Patch::allParameterDefinitions() const
	{
		return MKS50_Parameter::allParameterDefinitions;
	}

	// Funky. See page 65 of the PDF Manual
	std::vector<PackedDataFormatInfo> kToneFormatDefinition = {
		{ 0, 0, 4, MKS50_Parameter::VCF_KEY_FOLLOW },
		{ 0, 4, 4, MKS50_Parameter::DCO_AFTER_DEPTH },
		{ 1, 0, 4, MKS50_Parameter::VCA_AFTER_DEPTH },
		{ 1, 4, 4, MKS50_Parameter::VCF_AFTER_DEPTH },
		{ 2, 0, 4, MKS50_Parameter::BENDER_RANGE },
		{ 2, 4, 4, MKS50_Parameter::ENV_KEY_FOLLOW },
		{ 3, 0, 7, MKS50_Parameter::DCO_LFO_MOD_DEPTH },
		{ 4, 0, 7, MKS50_Parameter::DCO_ENV_MOD_DEPTH },
		{ 5, 0, 7, MKS50_Parameter::DCO_PW_PWM_DEPTH },
		{ 6, 0, 7, MKS50_Parameter::DCO_PWM_RATE },
		{ 7, 0, 7, MKS50_Parameter::VCF_CUTOFF_FREQ },
		{ 8, 0, 7, MKS50_Parameter::VCF_RESONANCE },
		{ 9, 0, 7, MKS50_Parameter::VCF_ENV_MOD_DEPTH },
		{ 10, 0, 7, MKS50_Parameter::VCF_LFO_MOD_DEPTH },
		{ 11, 0, 7, MKS50_Parameter::VCA_LEVEL },
		{ 12, 0, 7, MKS50_Parameter::LFO_RATE },
		{ 13, 0, 7, MKS50_Parameter::LFO_DELAY_TIME },
		{ 14, 0, 7, MKS50_Parameter::ENV_T1 },
		{ 15, 0, 7, MKS50_Parameter::ENV_L1 },
		{ 16, 0, 7, MKS50_Parameter::ENV_T2 },
		{ 17, 0, 7, MKS50_Parameter::ENV_L2 },
		{ 18, 0, 7, MKS50_Parameter::ENV_T3 },
		{ 19, 0, 7, MKS50_Parameter::ENV_L3 },
		{ 20, 0, 7, MKS50_Parameter::ENV_T4 },

		// Individual bits from here that need to be assembled to get the real value
		{ 4, 7, 1, MKS50_Parameter::CHORUS },
		{ 5, 7, 1, MKS50_Parameter::DCO_ENV_MODE, 1 },
		{ 6, 7, 1, MKS50_Parameter::DCO_ENV_MODE, 0 },
		{ 7, 7, 1, MKS50_Parameter::VCF_ENV_MODE, 1 },
		{ 8, 7, 1, MKS50_Parameter::VCF_ENV_MODE, 0 },
		{ 9, 7, 1, MKS50_Parameter::VCA_ENV_MODE, 1 },
		{ 10, 7, 1, MKS50_Parameter::VCA_ENV_MODE, 0 },
		{ 11, 7, 1, MKS50_Parameter::DCO_WAVEFORM_SUB, 2 },
		{ 12, 7, 1, MKS50_Parameter::DCO_WAVEFORM_SUB, 1 },
		{ 13, 7, 1, MKS50_Parameter::DCO_WAVEFORM_SUB, 0 },
		{ 14, 7, 1, MKS50_Parameter::DCO_WAVEFORM_SAWTOOTH, 2 },
		{ 15, 7, 1, MKS50_Parameter::DCO_WAVEFORM_SAWTOOTH, 1 },
		{ 16, 7, 1, MKS50_Parameter::DCO_WAVEFORM_SAWTOOTH, 0 },
		{ 17, 7, 1, MKS50_Parameter::DCO_WAVEFORM_PULSE, 1 },
		{ 18, 7, 1, MKS50_Parameter::DCO_WAVEFORM_PULSE, 0 },
		{ 19, 7, 1, MKS50_Parameter::HPF_CUTOFF_FREQ, 1 },
		{ 20, 7, 1, MKS50_Parameter::HPF_CUTOFF_FREQ, 0 },
		{ 21, 7, 1, MKS50_Parameter::DCO_RANGE, 1 },
		{ 22, 7, 1, MKS50_Parameter::DCO_RANGE, 0 },
		{ 23, 7, 1, MKS50_Parameter::DCO_SUB_LEVEL, 1 },
		{ 24, 7, 1, MKS50_Parameter::DCO_SUB_LEVEL, 0 },
		{ 25, 7, 1, MKS50_Parameter::DCO_NOISE_LEVEL, 1 },
		{ 26, 7, 1, MKS50_Parameter::DCO_NOISE_LEVEL, 0 },
		{ 27, 6, 2, MKS50_Parameter::CHORUS_RATE, 0 },
		{ 28, 6, 2, MKS50_Parameter::CHORUS_RATE, 2 },
		{ 29, 6, 2, MKS50_Parameter::CHORUS_RATE, 4 },
		{ 30, 6, 2, MKS50_Parameter::CHORUS_RATE, 6 },
	};

	std::shared_ptr<MKS50_Patch> MKS50_Patch::createFromToneBLD(MidiProgramNumber programNumber, std::vector<uint8> const& bldData)
	{
		if (bldData.size() != 32) {
			jassert(false);
			return std::shared_ptr<MKS50_Patch>();
		}

		// Extract the patchName first, for me to check that the whole decoding and depackaging worked
		std::string patchName;
		for (int i = 21; i < 31; i++) {
			patchName.push_back(kPatchNameChar[bldData[i] & 0b00111111]);
		}

		// Build up the APR tone record, which will be our preferred internal format and the one to store stuff in the database
		// If I trust the documentation correctly, this is also a message the MKS-50 will recognize without the user pressing a front panel button
		auto aprBlock = PackedDataFormatInfo::applyMapping(kToneFormatDefinition, bldData, 36);

		return std::make_shared<MKS50_Patch>(programNumber, patchName, aprBlock);
	}

	std::shared_ptr<MKS50_Patch> MKS50_Patch::createFromToneDAT(MidiProgramNumber programNumber, std::vector<uint8> const& datData)
	{
		return createFromToneBLD(programNumber, datData);
	}

	std::shared_ptr<MKS50_Patch> MKS50_Patch::createFromToneAPR(MidiMessage const& message)
	{
		std::vector<uint8> aprData;
		std::copy(message.getSysExData() + 6, message.getSysExData() + 36 + 6, std::back_inserter(aprData));
		std::string name;
		for (int index = 36 + 6; index < 46 + 6; index++) {
			int c = message.getSysExData()[index];
			if (c >= 0 && c < 64) {
				name += MKS50_Patch::kPatchNameChar[c];
			}
			else {
				name += "!";
			}
		}
		return std::make_shared<MKS50_Patch>(MidiProgramNumber::fromZeroBase(0), name, aprData);
	}

}
