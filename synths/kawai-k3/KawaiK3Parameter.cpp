/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "KawaiK3Parameter.h"

#include "KawaiK3.h"
#include "KawaiK3Patch.h"
#include "KawaiK3WaveParameter.h"

#include "MidiHelpers.h"

#include <fmt/format.h>

namespace midikraft {

	std::vector<std::string> kWaveFormNames = {
		"off", "grand piano I", "bell", "strings", "e.bass", "oboe", "e.piano I", "organ", "brass I", "digital bell", "slap bass",
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

	std::vector<std::string> kChorusName = {
		"None", "Chorus I (slow choral/phase shift)", "Chorus II (combination slow/fast shift)", "Chorus III (medium, random shift)",
		"Tremolo (fast, deep shift)", "Chorus IV (ambiance 1)", "Chorus V (ambiance 2)", "Delay (short 40-60 ms)"
	};

	std::vector<std::shared_ptr<SynthParameterDefinition>> KawaiK3Parameter::allParameters = {
		std::make_shared<KawaiK3Parameter>("Osc1 Wave", OSC1_WAVE_SELECT, 1, 6, 0, 33),
		std::make_shared<KawaiK3Parameter>("Osc1 Range", OSC1_RANGE, 1, 2, 6, 0, 2),
		std::make_shared<KawaiK3Parameter>("Portamento Speed", PORTAMENTO_SPEED, 2, 7, 0, 99),
		std::make_shared<KawaiK3Parameter>("Portamento Switch", PORTAMENTO_SWITCH, 2, 1, 7, 0, 1),
		std::make_shared<KawaiK3Parameter>("Osc2 Wave", OSC2_WAVE_SELECT, 3, 6, 0, 33),
		std::make_shared<KawaiK3Parameter>("Osc2 Coarse", OSC2_COARSE, 4, 5, -24, 24),
		std::make_shared<KawaiK3Parameter>("Osc2 Fine", OSC2_FINE, 5, 4, -10, 10),
		std::make_shared<KawaiK3Parameter>("Osc Balance", OSC_BALANCE, 6, 4, -15, 15),
		std::make_shared<KawaiK3Parameter>("Auto Bend", OSC_AUTO_BEND, 7, 5, -31, 31),
		std::make_shared<KawaiK3Parameter>("Mono", MONO, 8, 1, 7, 0, 1),
		std::make_shared<KawaiK3Parameter>("Pitch Bend", PITCH_BEND, 8, 3, 0, 7),
		std::make_shared<KawaiK3Parameter>("Cutoff", CUTOFF, 9, 7, 0, 99),
		std::make_shared<KawaiK3Parameter>("Resonance", RESONANCE, 10, 5, 0, 31),
		std::make_shared<KawaiK3Parameter>("VCF Env", VCF_ENV, 11, 5, 0, 31),
		std::make_shared<KawaiK3Parameter>("VCF Attack", VCF_ATTACK, 12, 5, 0, 31),
		std::make_shared<KawaiK3Parameter>("VCF Decay", VCF_DECAY, 13, 5, 0, 31),
		std::make_shared<KawaiK3Parameter>("Low Cut", LOW_CUT, 19, 5, 0, 31), // Manual says sysex position 14
		std::make_shared<KawaiK3Parameter>("VCF Sustain", VCF_SUSTAIN, 14, 5, 0, 31), // Manual says sysex position 15
		std::make_shared<KawaiK3Parameter>("VCF Release", VCF_RELEASE, 15, 5, 0, 31), // Manual says sysex position 16
		std::make_shared<KawaiK3Parameter>("VCA Level", VCA_LEVEL, 16, 5, 0, 31), // Manual says sysex position 17
		std::make_shared<KawaiK3Parameter>("VCA Attack", VCA_ATTACK, 17, 5, 0, 31), // Manual says sysex position 18
		std::make_shared<KawaiK3Parameter>("VCA Decay", VCA_DECAY, 18, 5, 0, 31), // Manual says sysex position 19
		std::make_shared<KawaiK3Parameter>("VCA Sustain", VCA_SUSTAIN, 20, 5, 0, 31),
		std::make_shared<KawaiK3Parameter>("VCA Release", VCA_RELEASE, 21, 5, 0, 31),
		std::make_shared<KawaiK3Parameter>("LFO Shape", LFO_SHAPE, 22, 3, 0, 6), // Manual says range 1 to 7
		std::make_shared<KawaiK3Parameter>("LFO Speed", LFO_SPEED, 23, 7, 0, 99),
		std::make_shared<KawaiK3Parameter>("LFO Delay", LFO_DELAY, 24, 5, 0, 31),
		std::make_shared<KawaiK3Parameter>("LFO to Osc", LFO_OSC, 25, 5, 0, 31),
		std::make_shared<KawaiK3Parameter>("LFO to VCF", LFO_VCF, 26, 5, 0, 31),
		std::make_shared<KawaiK3Parameter>("LFO to VCA", LFO_VCA, 27, 5, 0, 31),
		std::make_shared<KawaiK3Parameter>("Velocity to VCA", VELOCITY_VCA, 28, 4, 4, 0, 15), // upper nibble in sysex
		std::make_shared<KawaiK3Parameter>("Velocity to VCF", VELOCITY_VCF, 28, 4, 0, 0, 15), // lower nibble in sysex
		std::make_shared<KawaiK3Parameter>("Pressure to VCF", PRESSURE_VCF, 29, 4, 4, 0, 15), // upper nibble in sysex
		std::make_shared<KawaiK3Parameter>("Pressure to Osc Balance", PRESSURE_OSC_BALANCE, 29, 4, 0, 0, 15), // lower nibble in sysex
		std::make_shared<KawaiK3Parameter>("Pressure to LFO to Osc", PRESSURE_LFO_OSC, 30, 4, 4, 0, 15), // upper nibble in sysex
		std::make_shared<KawaiK3Parameter>("Pressure to VCA", PRESSURE_VCA, 30, 4, 0, 0, 15), // lower nibble in sysex
		std::make_shared<KawaiK3Parameter>("Keytracking to VCF", KCV_VCF, 31, 4, -15, 15),
		std::make_shared<KawaiK3Parameter>("Keytracking to VCA", KCV_VCA, 32, 4, -15, 15),
		std::make_shared<KawaiK3Parameter>("Chorus", CHORUS, 33, 3, 0, 7),
		std::make_shared<KawaiK3Parameter>("Default Parameter", DEFAULT_PARAMETER, 34, 6, 0, 39),
		std::make_shared<KawaiK3HarmonicsParameters>(),
		std::make_shared<KawaiK3DrawbarParameters>(DrawbarOrgan::hammondDrawbars()[0]),
		std::make_shared<KawaiK3DrawbarParameters>(DrawbarOrgan::hammondDrawbars()[1]),
		std::make_shared<KawaiK3DrawbarParameters>(DrawbarOrgan::hammondDrawbars()[2]),
		std::make_shared<KawaiK3DrawbarParameters>(DrawbarOrgan::hammondDrawbars()[3]),
		std::make_shared<KawaiK3DrawbarParameters>(DrawbarOrgan::hammondDrawbars()[4]),
		std::make_shared<KawaiK3DrawbarParameters>(DrawbarOrgan::hammondDrawbars()[5]),
		std::make_shared<KawaiK3DrawbarParameters>(DrawbarOrgan::hammondDrawbars()[6]),
		std::make_shared<KawaiK3DrawbarParameters>(DrawbarOrgan::hammondDrawbars()[7]),
		std::make_shared<KawaiK3DrawbarParameters>(DrawbarOrgan::hammondDrawbars()[8]),
	};

	std::shared_ptr<KawaiK3Parameter> KawaiK3Parameter::findParameter(Parameter param)
	{
		for (auto p : allParameters) {
			auto k3param = std::dynamic_pointer_cast<KawaiK3Parameter>(p);
			if (k3param) {
				if (k3param->paramNo_ == param) {
					return k3param;
				}
			}
		}
		return nullptr;
	}

	KawaiK3Parameter::KawaiK3Parameter(std::string const& name, Parameter param, int sysexIndex, int bits, int minValue, int maxValue) :
		name_(name), paramNo_(param), sysexIndex_(sysexIndex), sysexShift_(0), sysexBits_(bits), minValue_(minValue), maxValue_(maxValue)
	{
	}

	KawaiK3Parameter::KawaiK3Parameter(std::string const& name, Parameter param, int sysexIndex, int bits, int shift, int minValue, int maxValue) :
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

	std::string KawaiK3Parameter::valueInPatchToText(DataFile const& patch) const
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
                if (value >= 0 && value < static_cast<int>(kWaveFormNames.size())) {
                    return kWaveFormNames[static_cast<size_t>(value)];
                }
                else {
                    return "invalid wave form";
                }
			}
			if (paramNo_ == OSC1_RANGE) {
                if (value >= 0 && value < static_cast<int>(kRangeNames.size())) {
                    return kRangeNames[static_cast<size_t>(value)];
                }
                else
                {
                    return "invalid OSC range";
                }
			}
			if (paramNo_ == LFO_SHAPE) {
                if (value >= 0 && value < static_cast<int>(kLfoName.size())) {
                    return kLfoName[static_cast<size_t>(value)];
                }
                else {
                    return "invalid LFO shape";
                }
			}
			if (paramNo_ == CHORUS) {
                if (value >= 0 && value < static_cast<int>(kChorusName.size())) {
                    return kChorusName[static_cast<size_t>(value)];
                }
                else {
                    return "invalid chorus name";
                }
			}
			return fmt::format("{}", value);
		}
		return "invalid";
	}

	std::shared_ptr<TypedNamedValue> KawaiK3Parameter::makeTypedNamedValue()
	{
		switch (type()) {
		case midikraft::SynthParameterDefinition::ParamType::INT:
			return std::make_shared<TypedNamedValue>(name(), "KawaiK3", 0, minValue(), maxValue());
		case midikraft::SynthParameterDefinition::ParamType::LOOKUP: {
			std::map<int, std::string> lookup;
			for (int i = minValue(); i <= maxValue(); i++) {
				lookup.emplace(i, valueAsText(i));
			}
			return std::make_shared<TypedNamedValue>(name(), "KawaiK3", 0, lookup);
		}
        case midikraft::SynthParameterDefinition::ParamType::INT_ARRAY:
            // fall through
        case midikraft::SynthParameterDefinition::ParamType::LOOKUP_ARRAY:
                // fall through
		default:
			jassertfalse;
		}
		return nullptr;
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

	bool KawaiK3Parameter::valueInPatch(DataFile const& patch, int& outValue) const
	{
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

	void KawaiK3Parameter::setInPatch(DataFile& patch, int value) const
	{
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

	std::vector<MidiMessage>  KawaiK3Parameter::setValueMessages(std::shared_ptr<DataFile> const patch, Synth const* synth) const
	{
		auto k3 = dynamic_cast<KawaiK3 const*>(synth);
		jassert(k3);

		int paramValue;
		if (valueInPatch(*patch, paramValue)) {
			return setValueMessages(k3, paramValue);
		}
		jassertfalse;
		return {};
	}

	std::vector<MidiMessage> KawaiK3Parameter::setValueMessages(KawaiK3 const* k3, int paramValue) const
	{
		uint8 highNibble, lowNibble;
		if (minValue() < 0) {
			int clampedValue = std::min(std::max(paramValue, minValue()), maxValue());

			// Now, the K3 unfortunately uses a sign bit for the negative values, which makes it completely impossible to be used directly with the BCR2000
			if (clampedValue < 0) {
				highNibble = (((-clampedValue) & 0xFF) | 0x80) >> 4;
				lowNibble = (-clampedValue) & 0x0F;
			}
			else {
				highNibble = (clampedValue & 0xF0) >> 4;
				lowNibble = (clampedValue & 0x0F);
			}
		}
		else {
			// Just clamp to the min max range
			int correctedValue = std::min(std::max(paramValue, minValue()), maxValue());
			highNibble = (correctedValue & 0xF0) >> 4;
			lowNibble = (correctedValue & 0x0F);
		}

		// Now build the sysex message (p. 48 of the K3 manual)
		auto dataBlock = k3->buildSysexFunction(KawaiK3::PARAMETER_SEND, (uint8)paramNo());
		dataBlock.push_back(highNibble);
		dataBlock.push_back(lowNibble);
		return { MidiMessage::createSysExMessage(&dataBlock[0], static_cast<int>(dataBlock.size())) };
	}

	bool KawaiK3Parameter::messagesMatchParameter(std::vector<juce::MidiMessage> const& messages, int& outNewValue) const
	{
		for (auto message : messages) {
			if (message.isController()) {
				if (message.getControllerNumber() == paramNo_) {
					// Ah, that's us
					int value = message.getControllerValue();
					if (minValue_ < 0) {
						value = value + minValue_;
					}
					outNewValue = value;
					return true;
				}
			}
		}
		return false;
	}

	std::vector<juce::MidiMessage> KawaiK3Parameter::createParameterMessages(int newValue, MidiChannel channel) const
	{
		// For params with negative values, we offset!
		if (minValue() < 0) {
			newValue = newValue - minValue();
		}
		// As the K3 has only 39 parameters, we use CC 1..39 to map these. Simple enough
		return { MidiMessage::controllerEvent(channel.toOneBasedInt(), paramNo_, newValue) };
	}

	int KawaiK3Parameter::findWave(std::string shapename)
	{
		for (size_t i = 0; i < kWaveFormNames.size(); i++) {
			if (kWaveFormNames[i] == shapename)
				return static_cast<int>(i);
		}
		return -1;
	}

	std::string KawaiK3Parameter::waveName(int waveNo)
	{
		if (waveNo > 0 && waveNo < 34) {
			return kWaveFormNames[static_cast<size_t>(waveNo)];
		}
		return "invalid wave no";
	}

}
