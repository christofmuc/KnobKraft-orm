/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/


#include "MKS80_Parameter.h"

#include "Patch.h"
#include "MKS80_Patch.h"

#include <fmt/format.h>

namespace midikraft {

	// The parameter definition defines these for the lower section by default, but as the MKS80 is layered (or split in half), all of these also exist for the upper section!
	std::vector<std::shared_ptr<SynthParameterDefinition>> MKS80_Parameter::allParameterDefinitions = {
		std::make_shared<MKS80_Parameter>(TONE, LFO1_RATE, "LFO1_RATE", 0, 100),
		std::make_shared<MKS80_Parameter>(TONE, LFO1_DELAY_TIME, "LFO1_DELAY_TIME", 0, 100),
		std::make_shared<MKS80_Parameter>(MKS80_Parameter(TONE, LFO1_WAVEFORM, "LFO1_WAVEFORM", 0, 3, { {0, "Random Wave"}, { 1, "Square Wave" }, { 2, "Sawtooth Wave"}, {3, "Triangle Wave"} })),
		std::make_shared<MKS80_Parameter>(TONE, VCO_MOD_LFO1_DEPTH, "VCO_MOD_LFO1_DEPTH", 0, 100),
		std::make_shared<MKS80_Parameter>(TONE, VCO_MOD_ENV1_DEPTH, "VCO_MOD_ENV1_DEPTH", 0, 100),
		std::make_shared<MKS80_Parameter>(TONE, PULSE_WIDTH, "PULSE_WIDTH", 0, 100),
		std::make_shared<MKS80_Parameter>(TONE, PULSE_WIDTH_MOD, "PULSE_WIDTH_MOD", 0, 100),
		std::make_shared<MKS80_Parameter>(MKS80_Parameter(TONE, PWM_MODE_SELECT, "PWM_MODE_SELECT", 0, 2, { {0, "Keyboard"}, { 1, "LFO-1" }, { 2, "ENV-1"} })),
		std::make_shared<MKS80_Parameter>(MKS80_Parameter(TONE, PWM_POLARITY, "PWM_POLARITY", 0, 1, { {0, "Inverted"}, { 1, "Normal" } })),
		std::make_shared<MKS80_Parameter>(TONE, VCO_KEY_FOLLOW, "VCO_KEY_FOLLOW", 0, 100),
		std::make_shared<MKS80_Parameter>(MKS80_Parameter(TONE, VCO_SELECT, "VCO_SELECT", 0, 2, { {0, "VCO-2"}, { 1, "Off" }, { 2, "VCO-1"} })),
		std::make_shared<MKS80_Parameter>(TONE, XMOD_MANUAL_DEPTH, "XMOD_MANUAL_DEPTH", 0, 100),
		std::make_shared<MKS80_Parameter>(TONE, XMOD_ENV1_DEPTH, "XMOD_ENV1_DEPTH", 0, 100),
		std::make_shared<MKS80_Parameter>(MKS80_Parameter(TONE, XMOD_POLARITY, "XMOD_POLARITY", 0, 1, { {0, "Inverted"}, { 1, "Normal" } })),
		std::make_shared<MKS80_Parameter>(MKS80_Parameter(TONE, VCO1_MOD, "VCO1_MOD", 0, 2, { {0, "Inverted"}, { 1, "Off" }, { 2, "Normal"} })),
		std::make_shared<MKS80_Parameter>(TONE, VCO1_RANGE, "VCO1_RANGE", 36, 84),
		std::make_shared<MKS80_Parameter>(MKS80_Parameter(TONE, VCO1_WAVEFORM, "VCO1_WAVEFORM", 0, 3, { {0, "Square Wave"}, { 1, "Pulse Wave" }, { 2, "Sawtooth Wave"}, {3, "Triangle Wave"} })),
		std::make_shared<MKS80_Parameter>(MKS80_Parameter(TONE, VCO_SYNC, "VCO_SYNC", 0, 2, { {0, "VCO-2 -> VCO-1"}, { 1, "Off" }, { 2, "VCO-1 -> VCO-2"} })),
		std::make_shared<MKS80_Parameter>(MKS80_Parameter(TONE, VCO2_MOD, "VCO2_MOD", 0, 2, { {0, "Inverted"}, { 1, "Off" }, { 2, "Normal"} })),
		std::make_shared<MKS80_Parameter>(TONE, VCO2_RANGE, "VCO2_RANGE", 36, 100), //TODO Ugh, it actually allows 36-84 or 100
		std::make_shared<MKS80_Parameter>(TONE, VCO_FINE_TUNE, "VCO_FINE_TUNE", 0, 100),
		std::make_shared<MKS80_Parameter>(MKS80_Parameter(TONE, VCO2_WAVEFORM, "VCO2_WAVEFORM", 0, 3, { {0, "Noise"}, { 1, "Pulse Wave" }, { 2, "Sawtooth Wave"}, {3, "Triangle Wave"} })),
		std::make_shared<MKS80_Parameter>(TONE, MIXER, "MIXER", 0, 100),
		std::make_shared<MKS80_Parameter>(TONE, HPF_CUTOFF_FREQ, "HPF_CUTOFF_FREQ", 0, 100),
		std::make_shared<MKS80_Parameter>(TONE, VCF_CUTOFF_FREQ, "VCF_CUTOFF_FREQ", 0, 100),
		std::make_shared<MKS80_Parameter>(TONE, VCF_RESONANCE, "VCF_RESONANCE", 0, 100),
		std::make_shared<MKS80_Parameter>(MKS80_Parameter(TONE, VCF_ENV_SELECT, "VCF_ENV_SELECT", 0, 1, { {0, "ENV-2"}, { 1, "ENV-1" } })),
		std::make_shared<MKS80_Parameter>(MKS80_Parameter(TONE, VCF_ENV_POLARITY, "VCF_ENV_POLARITY", 0, 1, { {0, "Inverted"}, { 1, "Normal" } })),
		std::make_shared<MKS80_Parameter>(TONE, VCF_MOD_ENV_DEPTH, "VCF_MOD_ENV_DEPTH", 0, 100),
		std::make_shared<MKS80_Parameter>(TONE, VCF_MOD_LFO1_DEPTH, "VCF_MOD_LFO1_DEPTH", 0, 100),
		std::make_shared<MKS80_Parameter>(TONE, VCF_KEY_FOLLOW, "VCF_KEY_FOLLOW", 0, 100),
		std::make_shared<MKS80_Parameter>(TONE, VCA_ENV2, "VCA_ENV2", 0, 100),
		std::make_shared<MKS80_Parameter>(TONE, VCA_MOD_LFO1_DEPTH, "VCA_MOD_LFO1_DEPTH", 0, 100),
		std::make_shared<MKS80_Parameter>(TONE, DYNAMICS_TIME, "DYNAMICS_TIME", 0, 100),
		std::make_shared<MKS80_Parameter>(TONE, DYNAMICS_LEVEL, "DYNAMICS_LEVEL", 0, 100),
		std::make_shared<MKS80_Parameter>(MKS80_Parameter(TONE, ENV_RESET, "ENV_RESET", 0, 1, { {0, "Off"}, { 1, "On" } })),
		std::make_shared<MKS80_Parameter>(MKS80_Parameter(TONE, ENV1_DYNAMICS, "ENV1_DYNAMICS", 0, 1, { {0, "Off"}, { 1, "On" } })),
		std::make_shared<MKS80_Parameter>(TONE, ENV1_ATTACK, "ENV1_ATTACK", 0, 100),
		std::make_shared<MKS80_Parameter>(TONE, ENV1_DECAY, "ENV1_DECAY", 0, 100),
		std::make_shared<MKS80_Parameter>(TONE, ENV1_SUSTAIN, "ENV1_SUSTAIN", 0, 100),
		std::make_shared<MKS80_Parameter>(TONE, ENV1_RELEASE, "ENV1_RELEASE", 0, 100),
		std::make_shared<MKS80_Parameter>(TONE, ENV1_KEY_FOLLOW, "ENV1_KEY_FOLLOW", 0, 100),
		std::make_shared<MKS80_Parameter>(MKS80_Parameter(TONE, ENV2_DYNAMICS, "ENV2_DYNAMICS", 0, 1, { {0, "Off"}, { 1, "On" } })),
		std::make_shared<MKS80_Parameter>(TONE, ENV2_ATTACK, "ENV2_ATTACK", 0, 100),
		std::make_shared<MKS80_Parameter>(TONE, ENV2_DECAY, "ENV2_DECAY", 0, 100),
		std::make_shared<MKS80_Parameter>(TONE, ENV2_SUSTAIN, "ENV2_SUSTAIN", 0, 100),
		std::make_shared<MKS80_Parameter>(TONE, ENV2_RELEASE, "ENV2_RELEASE", 0, 100),
		std::make_shared<MKS80_Parameter>(TONE, ENV2_KEY_FOLLOW, "ENV2_KEY_FOLLOW", 0, 100),
		std::make_shared<MKS80_Parameter>(MKS80_Parameter(PATCH, KEY_MODE_SELECT, "KEY_MODE_SELECT" ,0, 3, { {0, "Dual"}, { 1, "Split-1" }, { 2, "Split-2"}, {3, "Whole"} })),
		std::make_shared<MKS80_Parameter>(PATCH, SPLIT_POINT, "SPLIT_POINT", 21, 108), // Note number
		std::make_shared<MKS80_Parameter>(PATCH, BALANCE, "BALANCE", 0, 100),
		std::make_shared<MKS80_Parameter>(PATCH, TONE_NUMBER, "TONE_NUMBER", 0, 63),
		std::make_shared<MKS80_Parameter>(MKS80_Parameter(PATCH, OCTAVE_SHIFT, "OCTAVE_SHIFT" ,0, 4, { {0, "2 Oct. down"}, { 1, "1 Oct. down" }, { 2, "Normal"}, {3, "1 Oct. up"}, {4, "2 Oct. up"} })),
		std::make_shared<MKS80_Parameter>(MKS80_Parameter(PATCH, ASSIGN_MODE_SELECT, "ASSIGN_MODE_SELECT" ,0, 4, { {0, "Solo"}, { 1, "Unison-1" }, { 2, "Unison-2"}, {3, "1 Oct. up"}, {4, "2 Oct. up"} })),
		std::make_shared<MKS80_Parameter>(PATCH, UNISON_DETUNE, "UNISON_DETUNE", 0, 100),
		std::make_shared<MKS80_Parameter>(MKS80_Parameter(PATCH, HOLD, "HOLD" ,0, 2, { {0, "Off"}, { 1, "On" }, { 2, "MIDI"} })),
		std::make_shared<MKS80_Parameter>(PATCH, GLIDE, "GLIDE", 0, 100),
		std::make_shared<MKS80_Parameter>(PATCH, BENDER_SENSIVITY, "BENDER_SENSIVITY", 0, 100),
		std::make_shared<MKS80_Parameter>(MKS80_Parameter(PATCH, VCO1_BEND, "VCO1_BEND" ,0, 2, { {0, "Off"}, { 1, "Normal" }, { 2, "Wide 2.5 Oct."} })),
		std::make_shared<MKS80_Parameter>(MKS80_Parameter(PATCH, VCO2_BEND, "VCO2_BEND" ,0, 2, { {0, "Off"}, { 1, "Normal" }, { 2, "Wide 2.5 Oct."} })),
		std::make_shared<MKS80_Parameter>(PATCH, AFTERTOUCH_SENSIVITY, "AFTERTOUCH_SENSIVITY", 0, 100),
		std::make_shared<MKS80_Parameter>(MKS80_Parameter(PATCH, AFTERTOUCH_MODE_SELECT, "AFTERTOUCH_MODE_SELECT" ,0, 1, { {0, "VCF Frequency"}, { 1, "VCO LFO-2 Mod (1 and 2)" } })),
		std::make_shared<MKS80_Parameter>(PATCH, LFO2_RATE, "LFO2_RATE", 0, 100),
	};

	std::shared_ptr<MKS80_Parameter> MKS80_Parameter::findParameter(ParameterType type, SynthSection section, int parameterIndex)
	{
		for (auto param : allParameterDefinitions) {
			auto mks = std::dynamic_pointer_cast<MKS80_Parameter>(param);
			if (mks) {
				if (mks->parameterType() == type && mks->paramIndex() == parameterIndex) {
					auto result = std::make_shared<MKS80_Parameter>(*mks); // Copy the canonical parameter definition, because we need to set the section
					result->setSection(section); // Determine if this parameter represents upper or lower section
					return result;
				}
			}
			else {
				jassert(false);
			}
		}
		return nullptr;
	}

	midikraft::SynthParameterDefinition::ParamType MKS80_Parameter::type() const
	{
		return SynthParameterDefinition::ParamType::INT;
	}

	std::string MKS80_Parameter::valueInPatchToText(DataFile const &patch) const
	{
		int value;
		if (valueInPatch(patch, value)) {
			return valueAsText(value);
		}
		return "illegal value";
	}

	void MKS80_Parameter::setInPatch(DataFile &patch, int value) const
	{
		auto mks80Patch = dynamic_cast<MKS80_Patch const &>(patch);
		if (value >= min_ && value <= max_) {
			mks80Patch.dataSection(paramType_, section_)[paramIndex_] = (uint8) value;
		}
		else {
			jassertfalse;
		}
	}

	std::shared_ptr<TypedNamedValue> MKS80_Parameter::makeTypedNamedValue()
	{
		//TODO - this code looks like it is not really synth specific
		switch (type()) {
		case midikraft::SynthParameterDefinition::ParamType::INT:
			return std::make_shared<TypedNamedValue>(name(), "MKS80", 0, minValue(), maxValue());
		case midikraft::SynthParameterDefinition::ParamType::LOOKUP: {
			std::map<int, std::string> lookup;
			for (int i = minValue(); i <= maxValue(); i++) {
				lookup.emplace(i, valueAsText(i));
			}
			return std::make_shared<TypedNamedValue>(name(), "MKS80", 0, lookup);
		}
		default:
			jassertfalse;
		}
		return nullptr;
	}

	MKS80_Parameter::MKS80_Parameter(ParameterType paramType, int paramIndex, std::string const &name, int min, int max) :
		paramType_(paramType), paramIndex_(paramIndex), paramName_(name), min_(min), max_(max), section_(SynthSection::LOWER)
	{
	}

	MKS80_Parameter::MKS80_Parameter(ParameterType paramType, int paramIndex, std::string const &name, int min, int max, std::map<int, std::string> const &valueLookup) :
		paramType_(paramType), paramIndex_(paramIndex), paramName_(name), min_(min), max_(max), valueLookup_(valueLookup), section_(SynthSection::LOWER)
	{
	}

	std::string MKS80_Parameter::name() const
	{
		return paramName_ + (section_ == LOWER ? "_L" : "_U");
	}

	std::string MKS80_Parameter::valueAsText(int value) const
	{
		if (valueLookup_.find(value) != valueLookup_.end()) {
			return valueLookup_.find(value)->second;
		}
		return fmt::format("{:d}", value);
	}

	int MKS80_Parameter::sysexIndex() const
	{
		throw std::logic_error("The method or operation is not implemented.");
	}

	std::string MKS80_Parameter::description() const
	{
		//TODO - I think this is a more verbose description of this parameter, e.g. for help texts. Could take it from manual
		return name();
	}

	int MKS80_Parameter::minValue() const
	{
		return min_;
	}

	int MKS80_Parameter::maxValue() const
	{
		return max_;
	}

	bool MKS80_Parameter::valueInPatch(DataFile const &patch, int &outValue) const
	{
		auto mks80Patch = dynamic_cast<MKS80_Patch const &>(patch);
		outValue = mks80Patch.dataSection(paramType_, section_)[paramIndex_];
		if (outValue >= min_ && outValue <= max_) {
			return true;
		}
		jassertfalse;
		return false;
	}

	void MKS80_Parameter::setSection(SynthSection section)
	{
		section_ = section;
	}

	int MKS80_Parameter::paramIndex() const
	{
		return paramIndex_;
	}

	MKS80_Parameter::ParameterType MKS80_Parameter::parameterType() const
	{
		return paramType_;
	}

	MKS80_Parameter::SynthSection MKS80_Parameter::section() const
	{
		return section_;
	}

}
