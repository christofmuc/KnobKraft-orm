#include "KorgDW8000Parameter.h"

#include "Patch.h"

#include <boost/format.hpp>


KorgDW8000Parameter::KorgDW8000Parameter(Parameter const paramIndex, std::string const &name, uint8 bits) : 
	paramIndex_(paramIndex), parameterName_(name), bits_(bits), maxValue_((1 << bits) -1)
{
}

KorgDW8000Parameter::KorgDW8000Parameter(Parameter const paramIndex, std::string const &name, uint8 bits, TValueLookup const &valueLookup) :
	paramIndex_(paramIndex), parameterName_(name), bits_(bits), maxValue_((1 << bits) - 1), valueLookup_(valueLookup)
{
}

KorgDW8000Parameter::KorgDW8000Parameter(Parameter const paramIndex, std::string const &name, uint8 bits, uint8 maxValue) :
	paramIndex_(paramIndex), parameterName_(name), bits_(bits), maxValue_(maxValue)
{
}

KorgDW8000Parameter::KorgDW8000Parameter(Parameter const paramIndex, std::string const &name, uint8 bits, uint8 maxValue, TValueLookup const &valueLookup) :
	paramIndex_(paramIndex), parameterName_(name), bits_(bits), maxValue_(maxValue), valueLookup_(valueLookup)
{
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

bool KorgDW8000Parameter::matchesController(int controllerNumber) const
{
	return false;
}

bool KorgDW8000Parameter::isActive(Patch const *patch) const
{
	return true;
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

int KorgDW8000Parameter::minValue() const
{
	return 0;
}

int KorgDW8000Parameter::maxValue() const
{
	return maxValue_;
}

TValueLookup cOctave = { { 0, "16""" },{ 1, "8""" },{ 2, "4""" } };
TValueLookup cWaveform = {};

std::vector<SynthParameterDefinition *> KorgDW8000Parameter::allParameters = {
	new KorgDW8000Parameter(OSC1_OCTAVE, "Osc 1 Octave", 2, 2, cOctave),
	new KorgDW8000Parameter(OSC1_WAVE_FORM, "Osc1 Wave Form", 4, cWaveform),
	new KorgDW8000Parameter(OSC1_LEVEL, "Osc 1 Level", 5),
	new KorgDW8000Parameter(AUTO_BEND_SELECT, "Auto Bend Select", 2, { {0, "Off"}, { 1, "Osc1" }, {2, "Osc2"}, {3, "Both"}}),
	new KorgDW8000Parameter(AUTO_BEND_MODE, "Auto Bend Mode", 1, {{0, "Up"}, {1, "Down"}}),
	new KorgDW8000Parameter(AUTO_BEND_TIME, "Auto Bend Time", 5),
	new KorgDW8000Parameter(AUTO_BEND_INTENSITY, "Auto Bend Intensity", 5),
	new KorgDW8000Parameter(OSC2_OCTAVE, "Osc 2 Octave", 2, 2, cOctave),
	new KorgDW8000Parameter(OSC2_WAVE_FORM, "Osc 2 Wave Form", 4, cWaveform),
	new KorgDW8000Parameter(OSC2_LEVEL,"Osc 2 Level", 5),
	new KorgDW8000Parameter(INTERVAL, "Osc 2 Interval", 3, 4, { {0, "1"}, {1, "-3 ST"}, {2, "3 ST"}, { 3, "4 ST"}, { 4, "5 ST" } }),
	new KorgDW8000Parameter(DETUNE, "Osc2 Detune", 3, 6),
	new KorgDW8000Parameter(NOISE_LEVEL, "Noise Level", 5),
	new KorgDW8000Parameter(ASSIGN_MODE, "Assign Mode", 2, {{0, "Poly 1"}, {1, "Poly 2"}, {2, "Unison 1"}, {3, "Unison 2"}}),
	new KorgDW8000Parameter(PARAMETER_NO_MEMORY, "Default Parameter", 6, 62),
	new KorgDW8000Parameter(CUTOFF, "Cutoff", 6),
	new KorgDW8000Parameter(RESONANCE, "Resonance", 5),
	new KorgDW8000Parameter(KBD_TRACK, "VCF Keyboard Tracking", 2, { {0, "0"}, {1, "1/4"}, {2, "1/2"}, {3, "Full"}}),
	new KorgDW8000Parameter(POLARITY, "VCF Envelope Polarity", 1, { {0, "Positive"}, { 1, "Negative"}}),
	new KorgDW8000Parameter(EG_INTENSITY, "VCF Env Intensity", 5),
	new KorgDW8000Parameter(VCF_ATTACK, "VCF Env Attack", 5),
	new KorgDW8000Parameter(VCF_DECAY, "VCF Env Decay", 5),
	new KorgDW8000Parameter(VCF_BREAK_POINT, "VCF Env Break Point", 5),
	new KorgDW8000Parameter(VCF_SLOPE, "VCF Env Slope", 5),
	new KorgDW8000Parameter(VCF_SUSTAIN, "VCF Env Sustain", 5),
	new KorgDW8000Parameter(VCF_RELEASE, "VCF Env Release", 5),
	new KorgDW8000Parameter(VCF_VELOCITY_SENSIVITY, "VCF Velocity Sensitivity", 3),
	new KorgDW8000Parameter(VCA_ATTACK, "VCA Env Attack", 5),
	new KorgDW8000Parameter(VCA_DECAY, "VCA Env Decay", 5),
	new KorgDW8000Parameter(VCA_BREAK_POINT, "VCA Env Break Point", 5),
	new KorgDW8000Parameter(VCA_SLOPE, "VCA Env Slope", 5),
	new KorgDW8000Parameter(VCA_SUSTAIN, "VCA Env Sustain", 5),
	new KorgDW8000Parameter(VCA_RELEASE, "VCA Env Release", 5),
	new KorgDW8000Parameter(VCA_VELOCITY_SENSIVITY, "VCA Velocity Sensitivity", 3),
	new KorgDW8000Parameter(MG_WAVE_FORM, "Modulation Wave Form", 2, {{0, "Triangle"}, {1, "Sawtooth"}, {2, "Inverse Saw"}, {3, "Square"}}),
	new KorgDW8000Parameter(MG_FREQUENCY, "Modulation Frequency", 5),
	new KorgDW8000Parameter(MG_DELAY, "Modulation Delay", 5),
	new KorgDW8000Parameter(MG_OSC, "Modulation Osc", 5),
	new KorgDW8000Parameter(MG_VCF, "Modulation VCF", 5),
	new KorgDW8000Parameter(BEND_OSC, "Pitch Bend Oscillators", 4, 12),
	new KorgDW8000Parameter(BEND_VCF, "Pitch Bend VCF", 1, {{0, "On"}, {1, "Off"}}),
	new KorgDW8000Parameter(DELAY_TIME, "Delay Time", 3),
	new KorgDW8000Parameter(DELAY_FACTOR, "Delay Factor", 4),
	new KorgDW8000Parameter(DELAY_FEEDBACK, "Delay Feedback", 4),
	new KorgDW8000Parameter(DELAY_FREQUENCY, "Delay Frequency", 5),
	new KorgDW8000Parameter(DELAY_INTENSITY, "Delay Intensity", 5),
	new KorgDW8000Parameter(DELAY_EFFECT_LEVEL, "Delay Effect Level", 4),
	new KorgDW8000Parameter(PORTAMENTO, "Portamento", 5),
	new KorgDW8000Parameter(AFTER_TOUCH_OSC_MG, "Aftertouch Osc Modulation", 2),
	new KorgDW8000Parameter(AFTER_TOUCH_VCF, "Aftertouch VCF Modulation", 2),
	new KorgDW8000Parameter(AFTER_TOUCH_VCA, "Aftertouch VCA Modulation", 2)
};

KorgDW8000Parameter * KorgDW8000Parameter::findParameter(Parameter param)
{
	for (auto parm : allParameters) {
		auto dw8000Param = dynamic_cast<KorgDW8000Parameter *>(parm);
		if (dw8000Param) {
			if (dw8000Param->paramIndex_ == param) {
				return dw8000Param;
			}
		}
	}
	return nullptr;
}
