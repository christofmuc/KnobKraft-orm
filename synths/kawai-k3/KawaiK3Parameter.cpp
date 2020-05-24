/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "KawaiK3Parameter.h"

#include "Patch.h"

#include <boost/format.hpp>

namespace midikraft {

	std::vector<std::string> kWaveFormNames = {
		"", "grand piano I", "bell", "strings", "e.bass", "oboe", "e.piano I", "organ", "brass I", "digital bell", "slap bass",
		"plucked string", "classic guitar", "hammered tine", "metallic wave", "vox humana", "sharp wave", "digital harmonics",
		"pipe organ", "wood bass", "resonant strings", "E.piano II", "jazz organ", "glocken", "oriental bell", "harpsichord",
		"trumpet", "sax", "grand piano II", "square", "sawtooth", "sine", "programmable", "white noise"
	};

	std::vector<std::string> kRangeNames = {
		"16""", "8""", "4"""
	};

	std::vector<std::string> kLfoName = {
		"Triangle", "Sawtooth", "Reverse sawtooth", "Square", "Inverted Square", "Random", "Chromatic random"
	};

	std::vector<std::string> kChrousName = {
		"None", "Chorus I (slow choral/phase shift)", "Chorus II (combination slow/fast shift)", "Chorus III (medium, random shift)",
		"Tremolo (fast, deep shift)", "Chorus IV (ambiance 1)", "Chorus V (ambiance 2)", "Delay (short 40-60 ms)"
	};

	std::vector<KawaiK3Parameter *> KawaiK3Parameter::allParameters = {
		new KawaiK3Parameter("Osc1 Wave", OSC1_WAVE_SELECT, 1, 6, 0, 33),
		new KawaiK3Parameter("Osc1 Range", OSC1_RANGE, 1, 2, 6, 0, 2),
		new KawaiK3Parameter("Portamento Speed", PORTAMENTO_SPEED, 2, 7, 0, 99),
		new KawaiK3Parameter("Portamento Switch", PORTAMENTO_SWITCH, 2, 1, 7, 0, 1),
		new KawaiK3Parameter("Osc2 Wave", OSC2_WAVE_SELECT, 3, 6, 0, 33),
		new KawaiK3Parameter("Osc2 Coarse", OSC2_COARSE, 4, 5, -24, 24),
		new KawaiK3Parameter("Osc2 Fine", OSC2_FINE, 5, 4, -10, 10),
		new KawaiK3Parameter("Osc Balance", OSC_BALANCE, 6, 4, -15, 15),
		new KawaiK3Parameter("Auto Bend", OSC_AUTO_BEND, 7, 5, -31, 31),
		new KawaiK3Parameter("Mono", MONO, 8, 1, 7, 0, 1),
		new KawaiK3Parameter("Pitch Bend", PITCH_BEND, 8, 3, 0, 7),
		new KawaiK3Parameter("Cutoff", CUTOFF, 9, 7, 0, 99),
		new KawaiK3Parameter("Resonance", RESONANCE, 10, 5, 0, 31),
		new KawaiK3Parameter("VCF Env", VCF_ENV, 11, 5, 0, 31),
		new KawaiK3Parameter("VCF Attack", VCF_ATTACK, 12, 5, 0, 31),
		new KawaiK3Parameter("VCF Decay", VCF_DECAY, 13, 5, 0, 31),
		new KawaiK3Parameter("Low Cut", LOW_CUT, 19, 5, 0, 31), // Manual says sysex position 14
		new KawaiK3Parameter("VCF Sustain", VCF_SUSTAIN, 14, 5, 0, 31), // Manual says sysex position 15
		new KawaiK3Parameter("VCF Release", VCF_RELEASE, 15, 5, 0, 31), // Manual says sysex position 16
		new KawaiK3Parameter("VCA Level", VCA_LEVEL, 16, 5, 0, 31), // Manual says sysex position 17
		new KawaiK3Parameter("VCA Attack", VCA_ATTACK, 17, 5, 0, 31), // Manual says sysex position 18
		new KawaiK3Parameter("VCA Decay", VCA_DECAY, 18, 5, 0, 31), // Manual says sysex position 19
		new KawaiK3Parameter("VCA Sustain", VCA_SUSTAIN, 20, 5, 0, 31),
		new KawaiK3Parameter("VCA Release", VCA_RELEASE, 21, 5, 0, 31),
		new KawaiK3Parameter("LFO Shape", LFO_SHAPE, 22, 3, 0, 6), // Manual says range 1 to 7
		new KawaiK3Parameter("LFO Speed", LFO_SPEED, 23, 7, 0, 99),
		new KawaiK3Parameter("LFO Delay", LFO_DELAY, 24, 5, 0, 31),
		new KawaiK3Parameter("LFO to Osc", LFO_OSC, 25, 5, 0, 31),
		new KawaiK3Parameter("LFO to VCF", LFO_VCF, 26, 5, 0, 31),
		new KawaiK3Parameter("LFO to VCA", LFO_VCA, 27, 5, 0, 31),
		new KawaiK3Parameter("Velocity to VCA", VELOCITY_VCA, 28, 4, 4, 0, 15), // upper nibble in sysex
		new KawaiK3Parameter("Velocity to VCF", VELOCITY_VCF, 28, 4, 0, 0, 15), // lower nibble in sysex
		new KawaiK3Parameter("Pressure to VCF", PRESSURE_VCF, 29, 4, 4, 0, 15), // upper nibble in sysex
		new KawaiK3Parameter("Pressure to Osc Balance", PRESSURE_OSC_BALANCE, 29, 4, 0, 0, 15), // lower nibble in sysex
		new KawaiK3Parameter("Pressure to LFO to Osc", PRESSURE_LFO_OSC, 30, 4, 4, 0, 15), // upper nibble in sysex
		new KawaiK3Parameter("Pressure to VCA", PRESSURE_VCA, 30, 4, 0, 0, 15), // lower nibble in sysex
		new KawaiK3Parameter("Keytracking to VCF", KCV_VCF, 31, 4, -15, 15),
		new KawaiK3Parameter("Keytracking to VCA", KCV_VCA, 32, 4, -15, 15),
		new KawaiK3Parameter("Chorus", CHORUS, 33, 3, 0, 7),
		new KawaiK3Parameter("Default Parameter", DEFAULT_PARAMETER, 34, 6, 0, 39)
	};

	KawaiK3Parameter *KawaiK3Parameter::findParameter(Parameter param)
	{
		for (auto p : allParameters) {
			if (p->paramNo_ == param) {
				return p;
			}
		}
		return nullptr;
	}

	KawaiK3Parameter::KawaiK3Parameter(std::string const &name, Parameter param, int sysexIndex, int bits, int minValue, int maxValue) :
		name_(name), paramNo_(param), sysexIndex_(sysexIndex), sysexShift_(0), sysexBits_(bits), minValue_(minValue), maxValue_(maxValue)
	{
	}

	KawaiK3Parameter::KawaiK3Parameter(std::string const &name, Parameter param, int sysexIndex, int bits, int shift, int minValue, int maxValue) :
		name_(name), paramNo_(param), sysexIndex_(sysexIndex), sysexShift_(shift), sysexBits_(bits), minValue_(minValue), maxValue_(maxValue)
	{
	}

	SynthParameterDefinition::ParamType KawaiK3Parameter::type() const
	{
		//TODO - this is fairly retro, needs to be refactored
		if (paramNo_ == OSC1_WAVE_SELECT || paramNo_ == OSC2_WAVE_SELECT || paramNo_ == OSC1_RANGE || paramNo_ == LFO_SHAPE || paramNo_ == CHORUS)
			return SynthParameterDefinition::ParamType::LOOKUP;
		return SynthParameterDefinition::ParamType::INT;
	}

	std::string KawaiK3Parameter::name() const
	{
		return name_;
	}

	std::string KawaiK3Parameter::valueInPatchToText(DataFile const &patch) const
	{
		int value;
		if (valueInPatch(patch, value)) {
			return valueAsText(value);
		}
		return "invalid";
	}

	std::string KawaiK3Parameter::valueAsText(int value) const
	{
		if (value >= minValue_ && value <= maxValue_) {
			if (paramNo_ == OSC1_WAVE_SELECT || paramNo_ == OSC2_WAVE_SELECT) {
				return kWaveFormNames[value];
			}
			if (paramNo_ == OSC1_RANGE) {
				return kRangeNames[value];
			}
			if (paramNo_ == LFO_SHAPE) {
				return kLfoName[value];
			}
			if (paramNo_ == CHORUS) {
				return kChrousName[value];
			}
			return (boost::format("%d") % value).str();
		}
		return "invalid";
	}

	int KawaiK3Parameter::sysexIndex() const
	{
		return sysexIndex_;
	}

	std::string KawaiK3Parameter::description() const
	{
		return name_;
	}

	int KawaiK3Parameter::maxValue() const
	{
		return maxValue_;
	}

	int KawaiK3Parameter::minValue() const
	{
		return minValue_;
	}

	bool KawaiK3Parameter::valueInPatch(DataFile const &patch, int &outValue) const
	{
		//TODO This is redundant with the KawaiK3Patch::value function
		int result = (patch.at(sysexIndex() - 1) >> shift()) & bitMask();
		if (minValue() < 0) {
			// This has a sign bit in bit 8
			if (patch.at(sysexIndex() - 1) & 0x80) {
				result = -result;
			}
		}
		outValue = result;
		return true;
	}

	void KawaiK3Parameter::setInPatch(DataFile &patch, int value) const
	{
		//TODO This is redundant with the KawaiK3Patch::setValue function - I think I should only keep this one
		//TODO - range checking for the parameter, we might specify values out of range?
		int currentValue = patch.at(sysexIndex() - 1);
		if (minValue() < 0) {
			if (value < 0) {
				uint8 cleanValue = (uint8)(currentValue & (~shiftedBitMask()));
				uint8 setValue = (uint8)(((abs(value) & bitMask()) << shift()) | 0x80); // Additionally set the sign bit
				patch.setAt(sysexIndex() - 1, cleanValue | setValue);
			}
			else {
				uint8 cleanValue = currentValue & (!bitMask()) & 0x7F;  // Additionally clear out the sign bit
				uint8 setValue = (uint8)((value & bitMask()) << shift());
				patch.setAt(sysexIndex() - 1, cleanValue | setValue);
			}
		}
		else {
			jassert(value >= 0);
			uint8 cleanValue = (uint8)(currentValue & (~shiftedBitMask()));
			uint8 setValue = (uint8)((value & bitMask()) << shift());
			patch.setAt(sysexIndex() - 1, cleanValue | setValue);
		}
	}

	KawaiK3Parameter::Parameter KawaiK3Parameter::paramNo() const
	{
		return paramNo_;
	}

	int KawaiK3Parameter::shift() const
	{
		return sysexShift_;
	}

	int KawaiK3Parameter::bits() const
	{
		return sysexBits_;
	}

	int KawaiK3Parameter::bitMask() const
	{
		return ((1 << sysexBits_) - 1);
	}

	int KawaiK3Parameter::shiftedBitMask() const
	{
		return (((uint16)((1 << sysexBits_) - 1)) << sysexShift_) & 0xff;
	}

	int KawaiK3Parameter::findWave(std::string shapename)
	{
		for (int i = 0; i < kWaveFormNames.size(); i++) {
			if (kWaveFormNames[i] == shapename)
				return i;
		}
		return -1;
	}

	std::string KawaiK3Parameter::waveName(int waveNo)
	{
		if (waveNo > 0 && waveNo < 34) {
			return kWaveFormNames[waveNo];
		}
		return "invalid wave no";
	}

}
