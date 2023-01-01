/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "MKS80_Patch.h"

#include "PackedDataFormatInfo.h"
#include "MKS80_Parameter.h"
#include "MidiHelpers.h"
#include "MKS80.h"

namespace midikraft {

	void MKS80_Patch::copyDataSection(std::map<APR_Section, std::vector<uint8>> const &data, std::vector<uint8> &result, MKS80_Patch::APR_Section section, int expectedLength) {
		if (data.find(section) == data.end()) {
			throw std::runtime_error("Invalid argument: Missing section in APR data!");
		}
		if (data.find(section)->second.size() != (size_t) expectedLength) {
			throw std::runtime_error("Invalid argument: APR section has invalid length!");
		}
		std::copy(data.find(section)->second.cbegin(), data.find(section)->second.cend(), std::back_inserter(result));
	}

	MKS80_Patch::MKS80_Patch(MidiProgramNumber patchNumber, std::map<APR_Section, std::vector<uint8>> const &data) : Patch(DF_MKS80_PATCH), patchNumber_(patchNumber)
	{
		std::vector<uint8> aggregatedData;
		copyDataSection(data, aggregatedData, APR_Section::PATCH_UPPER, 15);
		copyDataSection(data, aggregatedData, APR_Section::PATCH_LOWER, 15);
		copyDataSection(data, aggregatedData, APR_Section::TONE_UPPER, 48);
		copyDataSection(data, aggregatedData, APR_Section::TONE_LOWER, 48);
		setData(aggregatedData);
	}

	MKS80_Patch::MKS80_Patch(MidiProgramNumber patchNumber, Synth::PatchData const &data) : Patch(DF_MKS80_PATCH, data), patchNumber_(patchNumber)
	{
	}

	MidiProgramNumber MKS80_Patch::patchNumber() const
	{
		return patchNumber_;
	}

	int MKS80_Patch::value(SynthParameterDefinition const &param) const
	{
		ignoreUnused(param);
		throw std::logic_error("The method or operation is not implemented.");
	}

	SynthParameterDefinition const & MKS80_Patch::paramBySysexIndex(int sysexIndex) const
	{
		ignoreUnused(sysexIndex);
		throw std::logic_error("The method or operation is not implemented.");
	}

	std::vector<std::shared_ptr<SynthParameterDefinition>> MKS80_Patch::allParameterDefinitions() const
	{
		return MKS80_Parameter::allParameterDefinitions;
	}

	juce::MidiMessage MKS80_Patch::buildAPRMessage(APR_Section section, MidiChannel channel) const
	{
		std::vector<uint8> sysex({ ROLAND_ID, static_cast<uint8>(MKS80_Operation_Code::APR), (uint8)channel.toZeroBasedInt(), MKS80_ID,
			(uint8)(static_cast<int>(section) & 0xF0), (uint8)(static_cast<int>(section) & 0x0F) });
		switch (section)
		{
		case MKS80_Patch::APR_Section::PATCH_UPPER:
			std::copy(data().cbegin(), data().cbegin() + 15, std::back_inserter(sysex));
			break;
		case MKS80_Patch::APR_Section::PATCH_LOWER:
			std::copy(data().cbegin() + 15, data().cbegin() + 30, std::back_inserter(sysex));
			break;
		case MKS80_Patch::APR_Section::TONE_UPPER:
			std::copy(data().cbegin() + 30, data().cbegin() + 30 + 48, std::back_inserter(sysex));
			break;
		case MKS80_Patch::APR_Section::TONE_LOWER:
			std::copy(data().cbegin() + 30 + 48, data().cend(), std::back_inserter(sysex));
			break;
		}
		return MidiHelpers::sysexMessage(sysex);
	}

	bool MKS80_Patch::isValidAPRSection(int section)
	{
		switch (static_cast<APR_Section>(section)) {
		case APR_Section::PATCH_UPPER:
		case APR_Section::PATCH_LOWER:
		case APR_Section::TONE_UPPER:
		case APR_Section::TONE_LOWER:
			return true;
		}
		return false;
	}

	// Funky. See the PDF Manual - the data in the patch blocks is very different from the APR format
	// This can be used to convert the 39 bytes of the tone data in DAT format into the 48 bytes in APR format
	std::vector<PackedDataFormatInfo> kMKS80ToneFormatDefinition = {
		{ 0 , 0, 7, MKS80_Parameter::LFO1_RATE },
		{ 1 , 0, 7, MKS80_Parameter::LFO1_DELAY_TIME },
		{ 2 , 0, 7, MKS80_Parameter::VCO_MOD_LFO1_DEPTH },
		{ 3 , 0, 7, MKS80_Parameter::VCO_MOD_ENV1_DEPTH },
		{ 4 , 0, 7, MKS80_Parameter::PULSE_WIDTH },
		{ 5 , 0, 7, MKS80_Parameter::PULSE_WIDTH_MOD },
		{ 6 , 0, 7, MKS80_Parameter::VCO_KEY_FOLLOW },
		{ 7 , 0, 7, MKS80_Parameter::XMOD_MANUAL_DEPTH },
		{ 8 , 0, 7, MKS80_Parameter::XMOD_ENV1_DEPTH },
		{ 9 , 0, 7, MKS80_Parameter::VCO1_RANGE, [](uint8 data) { return (uint8)(data + 36); } },
		{10 , 0, 7, MKS80_Parameter::VCO2_RANGE, [](uint8 data) {
			if (data == 0) return (uint8)0;
			if (data == 50) return (uint8)100;
			return (uint8)(data + 35);
			}
		},
		{11 , 0, 7, MKS80_Parameter::VCO_FINE_TUNE },
		{12 , 0, 7, MKS80_Parameter::MIXER },
		{13 , 0, 7, MKS80_Parameter::HPF_CUTOFF_FREQ },
		{14 , 0, 7, MKS80_Parameter::VCF_CUTOFF_FREQ },
		{15 , 0, 7, MKS80_Parameter::VCF_RESONANCE },
		{16 , 0, 7, MKS80_Parameter::VCF_MOD_ENV_DEPTH },
		{17 , 0, 7, MKS80_Parameter::VCF_MOD_LFO1_DEPTH },
		{18 , 0, 7, MKS80_Parameter::VCF_KEY_FOLLOW },
		{19 , 0, 7, MKS80_Parameter::VCA_ENV2 /* VA_ENV2_LEVEL in the manual */ },
		{20 , 0, 7, MKS80_Parameter::VCA_MOD_LFO1_DEPTH },
		{21 , 0, 7, MKS80_Parameter::DYNAMICS_TIME },
		{22 , 0, 7, MKS80_Parameter::DYNAMICS_LEVEL },
		{23 , 0, 7, MKS80_Parameter::ENV1_ATTACK },
		{24 , 0, 7, MKS80_Parameter::ENV1_DECAY },
		{25 , 0, 7, MKS80_Parameter::ENV1_SUSTAIN },
		{26 , 0, 7, MKS80_Parameter::ENV1_RELEASE },
		{27 , 0, 7, MKS80_Parameter::ENV1_KEY_FOLLOW },
		{28 , 0, 7, MKS80_Parameter::ENV2_ATTACK },
		{29 , 0, 7, MKS80_Parameter::ENV2_DECAY },
		{30 , 0, 7, MKS80_Parameter::ENV2_SUSTAIN },
		{31 , 0, 7, MKS80_Parameter::ENV2_RELEASE },
		{32 , 0, 7, MKS80_Parameter::ENV2_KEY_FOLLOW },

				// Individual bits from here that need to be assembled to get the real value
				{33, 2, 2, MKS80_Parameter::PWM_MODE_SELECT },
				{33, 0, 2, MKS80_Parameter::LFO1_WAVEFORM },
				{34, 2, 2, MKS80_Parameter::VCO_SELECT },
				{34, 1, 1, MKS80_Parameter::XMOD_POLARITY },
				{34, 0, 1, MKS80_Parameter::PWM_POLARITY },
				{35, 2, 2, MKS80_Parameter::VCO2_MOD },
				{35, 0, 2, MKS80_Parameter::VCO1_MOD },
				{36, 3, 1, MKS80_Parameter::ENV2_DYNAMICS },
				{36, 2, 1, MKS80_Parameter::ENV1_DYNAMICS },
				{36, 1, 1, MKS80_Parameter::VCF_ENV_POLARITY },
				{36, 0, 1, MKS80_Parameter::VCF_ENV_SELECT },
				{37, 2, 2, MKS80_Parameter::VCO2_WAVEFORM },
				{37, 0, 2, MKS80_Parameter::VCO1_WAVEFORM },
				{38, 2, 1, MKS80_Parameter::ENV_RESET },
				{38, 0, 2, MKS80_Parameter::VCO_SYNC },
	};

	std::vector<juce::uint8> MKS80_Patch::toneFromDat(std::vector<uint8> const &dat)
	{
		return PackedDataFormatInfo::applyMapping(kMKS80ToneFormatDefinition, dat, 48);
	}

	std::vector<PackedDataFormatInfo> kMKS80PatchFormatDefinition = {
		{39 , 0, 3, MKS80_Parameter::KEY_MODE_SELECT },
		{40 , 0, 7, MKS80_Parameter::SPLIT_POINT }, //TODO - different number format in APR and DAT!
		{41 , 0, 7, MKS80_Parameter::BALANCE },
		{42 , 0, 6, MKS80_Parameter::TONE_NUMBER }, // Upper Tone number
		{43 , 0, 3, MKS80_Parameter::ASSIGN_MODE_SELECT},
		{44 , 0, 2, MKS80_Parameter::HOLD },
		{45 , 2, 2, MKS80_Parameter::VCO2_BEND },
		{46 , 0, 2, MKS80_Parameter::VCO1_BEND },
		{46 , 1, 3, MKS80_Parameter::OCTAVE_SHIFT },
		{46 , 0, 1, MKS80_Parameter::AFTERTOUCH_MODE_SELECT },
		{47 , 0, 7, MKS80_Parameter::UNISON_DETUNE },
		{48 , 0, 7, MKS80_Parameter::GLIDE },
		{49 , 0, 7, MKS80_Parameter::BENDER_SENSIVITY },
		{50 , 0, 7, MKS80_Parameter::AFTERTOUCH_SENSIVITY },
		{51 , 0, 7, MKS80_Parameter::LFO2_RATE },

		{39 , 0, 3, MKS80_Parameter::KEY_MODE_SELECT + 15},
		{40 , 0, 7, MKS80_Parameter::SPLIT_POINT + 15}, //TODO - different number format in APR and DAT!
		{41 , 0, 7, MKS80_Parameter::BALANCE + 15},
		{52 , 0, 6, MKS80_Parameter::TONE_NUMBER + 15}, // Lower Tone number
		{53 , 0, 3, MKS80_Parameter::ASSIGN_MODE_SELECT + 15},
		{54 , 0, 2, MKS80_Parameter::HOLD + 15},
		{55 , 2, 2, MKS80_Parameter::VCO2_BEND + 15},
		{55 , 0, 2, MKS80_Parameter::VCO1_BEND + 15},
		{56 , 1, 3, MKS80_Parameter::OCTAVE_SHIFT + 15},
		{56 , 0, 1, MKS80_Parameter::AFTERTOUCH_MODE_SELECT + 15},
		{57 , 0, 7, MKS80_Parameter::UNISON_DETUNE + 15},
		{58 , 0, 7, MKS80_Parameter::GLIDE + 15 },
		{59 , 0, 7, MKS80_Parameter::BENDER_SENSIVITY + 15 },
		{60 , 0, 7, MKS80_Parameter::AFTERTOUCH_SENSIVITY + 15 },
		{61 , 0, 7, MKS80_Parameter::LFO2_RATE + 15 },
	};

	std::vector<juce::uint8> MKS80_Patch::patchesFromDat(std::vector<uint8> const &dat)
	{
		return PackedDataFormatInfo::applyMapping(kMKS80PatchFormatDefinition, dat, 30);
	}

	juce::uint8 * MKS80_Patch::dataSection(APR_Section section)
	{
		switch (section) {
		case MKS80_Patch::APR_Section::PATCH_UPPER: return data_.data();
		case MKS80_Patch::APR_Section::PATCH_LOWER: return data_.data() + 15;
		case MKS80_Patch::APR_Section::TONE_UPPER: return data_.data() + 30;
		case MKS80_Patch::APR_Section::TONE_LOWER: return data_.data() + 30 + 48;
		}
		jassertfalse;
		throw new std::runtime_error("Invalid APR Section value");
	}

	juce::uint8 * MKS80_Patch::dataSection(MKS80_Parameter::ParameterType type, MKS80_Parameter::SynthSection section)
	{
		if (type == MKS80_Parameter::ParameterType::TONE) {
			return dataSection(section == MKS80_Parameter::SynthSection::LOWER ? MKS80_Patch::APR_Section::TONE_LOWER : MKS80_Patch::APR_Section::TONE_UPPER);
		}
		else {
			return dataSection(section == MKS80_Parameter::SynthSection::LOWER ? MKS80_Patch::APR_Section::PATCH_LOWER : MKS80_Patch::APR_Section::PATCH_UPPER);
		}
	}

}
