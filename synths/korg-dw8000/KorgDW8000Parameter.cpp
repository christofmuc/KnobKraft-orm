/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "KorgDW8000Parameter.h"

#include "Patch.h"

#include <boost/format.hpp>

namespace midikraft {


	KorgDW8000Parameter::KorgDW8000Parameter(Parameter const paramIndex, std::string const &name, int bits) :
		paramIndex_(paramIndex), parameterName_(name), bits_(bits), maxValue_((1 << bits) - 1)
	{
	}

	KorgDW8000Parameter::KorgDW8000Parameter(Parameter const paramIndex, std::string const &name, int bits, TValueLookup const &valueLookup) :
		paramIndex_(paramIndex), parameterName_(name), bits_(bits), maxValue_((1 << bits) - 1), valueLookup_(valueLookup)
	{
	}

	KorgDW8000Parameter::KorgDW8000Parameter(Parameter const paramIndex, std::string const &name, int bits, uint8 maxValue) :
		paramIndex_(paramIndex), parameterName_(name), bits_(bits), maxValue_(maxValue)
	{
	}

	KorgDW8000Parameter::KorgDW8000Parameter(Parameter const paramIndex, std::string const &name, int bits, uint8 maxValue, TValueLookup const &valueLookup) :
		paramIndex_(paramIndex), parameterName_(name), bits_(bits), maxValue_(maxValue), valueLookup_(valueLookup)
	{
	}

	SynthParameterDefinition::ParamType KorgDW8000Parameter::type() const
	{
		return SynthParameterDefinition::ParamType::INT;
	}

	std::string KorgDW8000Parameter::name() const
	{
		return parameterName_;
	}

	std::string KorgDW8000Parameter::valueAsText(int value) const
	{
		auto found = valueLookup_.find(value);
		if (found != valueLookup_.end()) {
			// How convenient, we can just use the string from the lookup table
			return found->second;
		}

		// Format as text
		return (boost::format("%d") % value).str();
	}

	int KorgDW8000Parameter::sysexIndex() const
	{
		return paramIndex_; // No mapping required for the Korg 
	}

	std::string KorgDW8000Parameter::description() const
	{
		return name();
	}

	std::string KorgDW8000Parameter::valueInPatchToText(Patch const &patch) const
	{
		int value;
		if (valueInPatch(patch, value)) {
			return valueAsText(value);
		}
		return "invalid";
	}

	bool KorgDW8000Parameter::valueInPatch(Patch const &patch, int &outValue) const
	{
		if (paramIndex_ >= patch.data().size()) {
			return false;
		}

		int value = patch.at(paramIndex_);
		if (value < 0 || value > maxValue_) {
			jassert(false);
			return false;
		}

		outValue = value;
		return true;
	}

	void KorgDW8000Parameter::setInPatch(Patch &patch, int value) const
	{
		patch.setAt(sysexIndex(), (uint8) value);
	}

	int KorgDW8000Parameter::minValue() const
	{
		return 0;
	}

	int KorgDW8000Parameter::maxValue() const
	{
		return maxValue_;
	}

	KorgDW8000Parameter::TValueLookup cOctave = { { 0, "16""" },{ 1, "8""" },{ 2, "4""" } };
	KorgDW8000Parameter::TValueLookup cWaveform = { { 0, "Sawtooth"}, { 1, "Square" }, { 2, "Piano" }, { 3, "Electric piano 1" }, { 4, "Electric piano 2" }, { 5, "Clavinet" },
		{ 6, "Organ" }, { 7, "Brass" }, { 8, "Sax" }, { 9, "Violin" }, { 10, "Guitar" }, { 11, "Electric guitar" }, { 12, "Bass" }, { 13, "Digital bass" }, { 14, "Bell and whistle" }, { 15, "Sine" } };

	std::vector<std::shared_ptr<SynthParameterDefinition>> KorgDW8000Parameter::allParameters = {
		std::make_shared<KorgDW8000Parameter>(KorgDW8000Parameter(OSC1_OCTAVE, "Osc 1 Octave", 2, 2, cOctave)),
		std::make_shared<KorgDW8000Parameter>(KorgDW8000Parameter(OSC1_WAVE_FORM, "Osc1 Wave Form", 4, cWaveform)),
		std::make_shared<KorgDW8000Parameter>(OSC1_LEVEL, "Osc 1 Level", 5),
		std::make_shared<KorgDW8000Parameter>(KorgDW8000Parameter(AUTO_BEND_SELECT, "Auto Bend Select", 2, { {0, "Off"}, { 1, "Osc1" }, {2, "Osc2"}, {3, "Both"}})),
		std::make_shared<KorgDW8000Parameter>(KorgDW8000Parameter(AUTO_BEND_MODE, "Auto Bend Mode", 1, {{0, "Up"}, {1, "Down"}})),
		std::make_shared<KorgDW8000Parameter>(AUTO_BEND_TIME, "Auto Bend Time", 5),
		std::make_shared<KorgDW8000Parameter>(AUTO_BEND_INTENSITY, "Auto Bend Intensity", 5),
		std::make_shared<KorgDW8000Parameter>(KorgDW8000Parameter(OSC2_OCTAVE, "Osc 2 Octave", 2, 2, cOctave)),
		std::make_shared<KorgDW8000Parameter>(KorgDW8000Parameter(OSC2_WAVE_FORM, "Osc 2 Wave Form", 4, cWaveform)),
		std::make_shared<KorgDW8000Parameter>(OSC2_LEVEL,"Osc 2 Level", 5),
		std::make_shared<KorgDW8000Parameter>(KorgDW8000Parameter(INTERVAL, "Osc 2 Interval", 3, 4, { {0, "1"}, {1, "-3 ST"}, {2, "3 ST"}, { 3, "4 ST"}, { 4, "5 ST" } })),
		std::make_shared<KorgDW8000Parameter>(DETUNE, "Osc2 Detune", 3, (uint8) 6),
		std::make_shared<KorgDW8000Parameter>(NOISE_LEVEL, "Noise Level", 5),
		std::make_shared<KorgDW8000Parameter>(KorgDW8000Parameter(ASSIGN_MODE, "Assign Mode", 2, {{0, "Poly 1"}, {1, "Poly 2"}, {2, "Unison 1"}, {3, "Unison 2"}})),
		std::make_shared<KorgDW8000Parameter>(PARAMETER_NO_MEMORY, "Default Parameter", 6, (uint8) 62),
		std::make_shared<KorgDW8000Parameter>(CUTOFF, "Cutoff", 6),
		std::make_shared<KorgDW8000Parameter>(RESONANCE, "Resonance", 5),
		std::make_shared<KorgDW8000Parameter>(KorgDW8000Parameter(KBD_TRACK, "VCF Keyboard Tracking", 2, { {0, "0"}, {1, "1/4"}, {2, "1/2"}, {3, "Full"}})),
		std::make_shared<KorgDW8000Parameter>(KorgDW8000Parameter(POLARITY, "VCF Envelope Polarity", 1, { {0, "Positive"}, { 1, "Negative"}})),
		std::make_shared<KorgDW8000Parameter>(EG_INTENSITY, "VCF Env Intensity", 5),
		std::make_shared<KorgDW8000Parameter>(VCF_ATTACK, "VCF Env Attack", 5),
		std::make_shared<KorgDW8000Parameter>(VCF_DECAY, "VCF Env Decay", 5),
		std::make_shared<KorgDW8000Parameter>(VCF_BREAK_POINT, "VCF Env Break Point", 5),
		std::make_shared<KorgDW8000Parameter>(VCF_SLOPE, "VCF Env Slope", 5),
		std::make_shared<KorgDW8000Parameter>(VCF_SUSTAIN, "VCF Env Sustain", 5),
		std::make_shared<KorgDW8000Parameter>(VCF_RELEASE, "VCF Env Release", 5),
		std::make_shared<KorgDW8000Parameter>(VCF_VELOCITY_SENSIVITY, "VCF Velocity Sensitivity", 3),
		std::make_shared<KorgDW8000Parameter>(VCA_ATTACK, "VCA Env Attack", 5),
		std::make_shared<KorgDW8000Parameter>(VCA_DECAY, "VCA Env Decay", 5),
		std::make_shared<KorgDW8000Parameter>(VCA_BREAK_POINT, "VCA Env Break Point", 5),
		std::make_shared<KorgDW8000Parameter>(VCA_SLOPE, "VCA Env Slope", 5),
		std::make_shared<KorgDW8000Parameter>(VCA_SUSTAIN, "VCA Env Sustain", 5),
		std::make_shared<KorgDW8000Parameter>(VCA_RELEASE, "VCA Env Release", 5),
		std::make_shared<KorgDW8000Parameter>(VCA_VELOCITY_SENSIVITY, "VCA Velocity Sensitivity", 3),
		std::make_shared<KorgDW8000Parameter>(KorgDW8000Parameter(MG_WAVE_FORM, "Modulation Wave Form", 2, {{0, "Triangle"}, {1, "Sawtooth"}, {2, "Inverse Saw"}, {3, "Square"}})),
		std::make_shared<KorgDW8000Parameter>(MG_FREQUENCY, "Modulation Frequency", 5),
		std::make_shared<KorgDW8000Parameter>(MG_DELAY, "Modulation Delay", 5),
		std::make_shared<KorgDW8000Parameter>(MG_OSC, "Modulation Osc", 5),
		std::make_shared<KorgDW8000Parameter>(MG_VCF, "Modulation VCF", 5),
		std::make_shared<KorgDW8000Parameter>(BEND_OSC, "Pitch Bend Oscillators", 4, (uint8) 12),
		std::make_shared<KorgDW8000Parameter>(KorgDW8000Parameter(BEND_VCF, "Pitch Bend VCF", 1, {{0, "On"}, {1, "Off"}})),
		std::make_shared<KorgDW8000Parameter>(DELAY_TIME, "Delay Time", 3),
		std::make_shared<KorgDW8000Parameter>(DELAY_FACTOR, "Delay Factor", 4),
		std::make_shared<KorgDW8000Parameter>(DELAY_FEEDBACK, "Delay Feedback", 4),
		std::make_shared<KorgDW8000Parameter>(DELAY_FREQUENCY, "Delay Frequency", 5),
		std::make_shared<KorgDW8000Parameter>(DELAY_INTENSITY, "Delay Intensity", 5),
		std::make_shared<KorgDW8000Parameter>(DELAY_EFFECT_LEVEL, "Delay Effect Level", 4),
		std::make_shared<KorgDW8000Parameter>(PORTAMENTO, "Portamento", 5),
		std::make_shared<KorgDW8000Parameter>(AFTER_TOUCH_OSC_MG, "Aftertouch Osc Modulation", 2),
		std::make_shared<KorgDW8000Parameter>(AFTER_TOUCH_VCF, "Aftertouch VCF Modulation", 2),
		std::make_shared<KorgDW8000Parameter>(AFTER_TOUCH_VCA, "Aftertouch VCA Modulation", 2)
	};

	std::shared_ptr<KorgDW8000Parameter> KorgDW8000Parameter::findParameter(Parameter param)
	{
		for (auto parm : allParameters) {
			auto dw8000Param = std::dynamic_pointer_cast<KorgDW8000Parameter>(parm);
			if (dw8000Param) {
				if (dw8000Param->paramIndex_ == param) {
					return dw8000Param;
				}
			}
		}
		return nullptr;
	}

}
