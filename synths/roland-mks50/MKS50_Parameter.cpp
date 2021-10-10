/*
   Copyright (c) 2021 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "MKS50_Parameter.h"

#include "Patch.h"

#include <boost/format.hpp>

namespace midikraft {

	std::vector<std::shared_ptr<SynthParameterDefinition>> MKS50_Parameter::allParameterDefinitions = {
		std::make_shared<MKS50_Parameter>(MKS50_Parameter(TONE, DCO_ENV_MODE, "DCO Env Mode", 0, 3, { { 0, "Env normal" }, { 1, "Env inverted" },  { 2, "Env normal with dynamics" }, { 3, "Env inverted with dynamics" }})),
		std::make_shared<MKS50_Parameter>(MKS50_Parameter(TONE, VCF_ENV_MODE, "VCF Env Mode", 0, 3, { { 0, "Env normal" }, { 1, "Env inverted" },  { 2, "Env normal with dynamics" }, { 3, "Env inverted with dynamics" }})),
		std::make_shared<MKS50_Parameter>(MKS50_Parameter(TONE, VCA_ENV_MODE, "VCA Env Mode", 0, 3, { { 0, "Env" }, { 1, "Date" },  { 2, "Env with dynamics" }, { 3, "Date with dynamics" }})),
		std::make_shared<MKS50_Parameter>(TONE, DCO_WAVEFORM_PULSE, "DCO Waveform Pulse", 0, 3),
		std::make_shared<MKS50_Parameter>(TONE, DCO_WAVEFORM_SAWTOOTH, "DCO Waveform Sawtooth", 0, 5),
		std::make_shared<MKS50_Parameter>(TONE, DCO_WAVEFORM_SUB, "DCO Waveform Sub", 0, 5),
		std::make_shared<MKS50_Parameter>(MKS50_Parameter(TONE, DCO_RANGE, "DCO Range", 0, 3, {{ 0, "4'" }, { 1, "8'" }, { 2, "16'" }, { 3, "32'" }})),
		std::make_shared<MKS50_Parameter>(TONE, DCO_SUB_LEVEL, "DCO Sub Level", 0, 3),
		std::make_shared<MKS50_Parameter>(TONE, DCO_NOISE_LEVEL, "DCO Noise Level", 0, 3),
		std::make_shared<MKS50_Parameter>(TONE, HPF_CUTOFF_FREQ, "HPF Cutoff Freq", 0, 3),
		std::make_shared<MKS50_Parameter>(MKS50_Parameter(TONE, CHORUS, "Chorus", 0, 1, {{ 0, "Off" }, { 1, "On" }})),
		std::make_shared<MKS50_Parameter>(TONE, DCO_LFO_MOD_DEPTH, "DCO LFO Mod Depth", 0, 127),
		std::make_shared<MKS50_Parameter>(TONE, DCO_ENV_MOD_DEPTH, "DCO Env Mod Depth", 0, 127),
		std::make_shared<MKS50_Parameter>(TONE, DCO_AFTER_DEPTH, "DCO Aftertouch Depth", 0, 127),
		std::make_shared<MKS50_Parameter>(TONE, DCO_PW_PWM_DEPTH, "DCO PW/PWM Depth", 0, 127),
		std::make_shared<MKS50_Parameter>(TONE, DCO_PWM_RATE, "DCO PWM Rate", 0, 127),
		std::make_shared<MKS50_Parameter>(TONE, VCF_CUTOFF_FREQ, "VCF Cutoff", 0, 127),
		std::make_shared<MKS50_Parameter>(TONE, VCF_RESONANCE, "VCF Resonance", 0, 127),
		std::make_shared<MKS50_Parameter>(TONE, VCF_LFO_MOD_DEPTH, "LFO Mod Depth", 0, 127),
		std::make_shared<MKS50_Parameter>(TONE, VCF_ENV_MOD_DEPTH, "VCF Env Depth", 0, 127),
		std::make_shared<MKS50_Parameter>(TONE, VCF_KEY_FOLLOW, "VCF key follow", 0, 127),
		std::make_shared<MKS50_Parameter>(TONE, VCF_AFTER_DEPTH, "VCF Aftertouch Depth", 0, 127),
		std::make_shared<MKS50_Parameter>(TONE, VCA_LEVEL, "VCA Level", 0, 127),
		std::make_shared<MKS50_Parameter>(TONE, VCA_AFTER_DEPTH, "VCA Aftertouch Depth", 0, 127),
		std::make_shared<MKS50_Parameter>(TONE, LFO_RATE, "LFO Rate", 0, 127),
		std::make_shared<MKS50_Parameter>(TONE, LFO_DELAY_TIME, "LFO Delay Time", 0, 127),
		std::make_shared<MKS50_Parameter>(TONE, ENV_T1, "Attack time", 0, 127),
		std::make_shared<MKS50_Parameter>(TONE, ENV_L1, "Attach level", 0, 127),
		std::make_shared<MKS50_Parameter>(TONE, ENV_T2, "Break time", 0, 127),
		std::make_shared<MKS50_Parameter>(TONE, ENV_L2, "Break level", 0, 127),
		std::make_shared<MKS50_Parameter>(TONE, ENV_T3, "Decay time", 0, 127),
		std::make_shared<MKS50_Parameter>(TONE, ENV_L3, "Sustain level", 0, 127),
		std::make_shared<MKS50_Parameter>(TONE, ENV_T4, "Release time", 0, 127),
		std::make_shared<MKS50_Parameter>(TONE, ENV_KEY_FOLLOW, "Env key follow", 0, 127),
		std::make_shared<MKS50_Parameter>(TONE, CHORUS_RATE, "Chorus rate", 0, 127),
		std::make_shared<MKS50_Parameter>(TONE, BENDER_RANGE, "Bender range", 0, 12),
		std::make_shared<MKS50_Parameter>(PATCH, TONE_NUMBER, "Tone number" ,0, 127),
		std::make_shared<MKS50_Parameter>(PATCH, KEY_RANGE_LOW, "Key range low", 12, 108),
		std::make_shared<MKS50_Parameter>(PATCH, KEY_RANGE_HIGH, "Key range high", 13, 109),
		std::make_shared<MKS50_Parameter>(PATCH, PORTAMENTO_TIME, "Portamento time", 0, 127),
		std::make_shared<MKS50_Parameter>(MKS50_Parameter(PATCH, PORTAMENTO, "Portamento", 0, 1, {{ 0, "Off" }, { 1, "On" }})),
		std::make_shared<MKS50_Parameter>(PATCH, MODULATION_SENSIVITY, "Modulation sensitivity", 0, 127),
		std::make_shared<MKS50_Parameter>(PATCH, KEY_SHIFT, "Transpose", -12, 12),
		std::make_shared<MKS50_Parameter>(PATCH, VOLUME, "Volume", 0, 127),
		std::make_shared<MKS50_Parameter>(PATCH, DETUNE, "Detune", -64, 63),
		std::make_shared<MKS50_Parameter>(PATCH, MIDI_FUNCTION, "MIDI function", 0, 127),
		std::make_shared<MKS50_Parameter>(PATCH, MONO_BENDER_RATE, "Mono bender rate", 0, 12),
		std::make_shared<MKS50_Parameter>(PATCH, CHORD_MEMORY, "Chord memory", 0, 16),
		std::make_shared<MKS50_Parameter>(PATCH, KEY_ASSIGN_MODE, "Key assign mode", 0, 63),
	};

	MKS50_Parameter::MKS50_Parameter(ParameterType paramType, int paramIndex, std::string const& name, int min, int max) :
		paramType_(paramType), paramIndex_(paramIndex), paramName_(name), min_(min), max_(max)
	{
	}

	MKS50_Parameter::MKS50_Parameter(ParameterType paramType, int paramIndex, std::string const& name, int min, int max, std::map<int, std::string> const& valueLookup) :
		paramType_(paramType), paramIndex_(paramIndex), paramName_(name), min_(min), max_(max), valueLookup_(valueLookup)
	{
	}

	midikraft::SynthParameterDefinition::ParamType MKS50_Parameter::type() const
	{
		if (valueLookup_.size() > 0) {
			return SynthParameterDefinition::ParamType::LOOKUP;
		}
		return SynthParameterDefinition::ParamType::INT;
	}

	std::string MKS50_Parameter::valueInPatchToText(DataFile const& patch) const
	{
		int v = patch.data()[paramIndex_];
		return valueAsText(v);
	}

	std::string MKS50_Parameter::name() const
	{
		return paramName_;
	}

	std::string MKS50_Parameter::valueAsText(int value) const
	{
		if (valueLookup_.find(value) != valueLookup_.end()) {
			return valueLookup_.find(value)->second;
		}
		return (boost::format("%d") % value).str();
	}

	int MKS50_Parameter::sysexIndex() const
	{
		return paramIndex_;
	}

	std::string MKS50_Parameter::description() const
	{
		return paramName_;
	}

	int MKS50_Parameter::minValue() const
	{
		return min_;
	}

	int MKS50_Parameter::maxValue() const
	{
		return max_;
	}

}
