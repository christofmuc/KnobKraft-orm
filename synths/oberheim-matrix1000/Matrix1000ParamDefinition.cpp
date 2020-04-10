#include "Matrix1000ParamDefinition.h"

#include "Patch.h"

#include <set>
#include <boost/format.hpp>

Matrix1000Param Matrix1000ParamDefinition::id() const
{
	return paramId_;
}

int Matrix1000ParamDefinition::controller() const
{
	return controller_;
}

int Matrix1000ParamDefinition::bits() const
{
	return bits_;
}

int Matrix1000ParamDefinition::bitposition() const
{
	return bitposition_;
}

std::string Matrix1000ParamDefinition::name() const
{
	return description_;
}

std::string Matrix1000ParamDefinition::valueAsText(int value) const
{
	auto found = lookup_.find(value);
	if (found != lookup_.end()) {
		// How convenient, we can just use the string from the lookup table
		return found->second;
	}

	// Format as text
	return (boost::format("%d") % value).str();
}

int Matrix1000ParamDefinition::sysexIndex() const
{
	return sysexIndex_;
}

int Matrix1000ParamDefinition::minValue() const
{
	return 0;
}

int Matrix1000ParamDefinition::maxValue() const
{
	return 1 << (bits_ - 1);
}

bool Matrix1000ParamDefinition::matchesController(int controllerNumber) const
{
	return controllerNumber == controller_;
}

bool Matrix1000ParamDefinition::isActive(Patch const *patch) const
{
	// The simplest mechanism to test if a paramater is active is this:
	if (activeIfNonNull_) {
		// We just need to check if the value of this parameter is not null, then it is deemed to be active!
		int value;
		if (valueInPatch(*patch, value)) {
			return value != 0;
		}
		// Value not present, can't be active
		return false;
	}

	// Then, we might have a predicate to test activeness
	if (testActive_) {
		return testActive_(*patch);
	}

	// Else we must assume it is active, because we have no more information
	return true;
}

bool Matrix1000ParamDefinition::valueInPatch(Patch const &patch, int &outValue) const
{
	// For this to work, this parameter must have a sysex definition
	if (sysexIndex_ == -1 || sysexIndex_ >= patch.data().size()) {
		return false;
	}

	int value = patch.at(sysexIndex_);
	if (bitposition_ != -1) {
		// Oh, this parameter is a single bit. I need to mask out that bit only
		value = (value & (1 << bitposition_)) >> bitposition_;
	}
	else if (bits_ < 0) {
		// Surely I should be able to do this with a proper type cast?
		value = (int8)value;
	}
	outValue = value;
	return true;
}

SynthParameterDefinition const & Matrix1000ParamDefinition::param(Matrix1000Param id)
{
	// For now, just scan the table
	for (auto param : allDefinitions) {
		auto matrix100param = dynamic_cast<Matrix1000ParamDefinition *>(param);
		if (matrix100param->paramId_ == id) {
			return *param;
		}
	}
	throw new std::runtime_error("Invalid Matrix 1000 param ID");
}

std::string Matrix1000ParamDefinition::description() const
{
	return description_;
}

std::map<int, std::string> lfoWaveCodes = {
	{ 0 , "Triangle" },
	{ 1 , "Up Sawtooth" },
	{ 2 , "Down Sawtooth" },
	{ 3 , "Square" },
	{ 4 , "Random" },
	{ 5 , "Noise" },
	{ 6 , "Sampled Modulation" },
	{ 7 , "Not Used" },
};

std::map<int, std::string> modulationSourceCodes = {
	{ 0, "Unused Modulation" },
	{ 1, "Env 1" },
	{ 2, "Env 2" },
	{ 3, "Env 3" },
	{ 4, "LFO 1" },
	{ 5, "LFO 2" },
	{ 6, "Vibrato" },
	{ 7, "Ramp 1" },
	{ 8, "Ramp 2" },
	{ 9, "Keyboard" },
	{ 10, "Portamento" },
	{ 11, "Tracking Generator" },
	{ 12, "Keyboard Gate" },
	{ 13, "Velocity" },
	{ 14, "Release Velocity" },
	{ 15, "Pressure" },
	{ 16, "Pedal 1" },
	{ 17, "Pedal 2" },
	{ 18, "Lever 1" },
	{ 19, "Lever 2" },
	{ 20, "Lever 3" },
};

std::map<int, std::string> modulationDestinationCodes = {
	{ 0, "Unused Modulation" },
	{ 1, "DCO 1 Frequency" },
	{ 2, "DCO 1 Pulse Width" },
	{ 3, "DCO 1 Waveshape" },
	{ 4, "DCO 2 Frequency" },
	{ 5, "DCO 2 Pulse Width" },
	{ 6, "DCO 2 Waveshape" },
	{ 7, "Mix Level" },
	{ 8, "VCF FM Amount" },
	{ 9, "VCF Frequency" },
	{ 10, "VCF Resonance" },
	{ 11, "VCA 1 Level" },
	{ 12, "VCA 2 Level" },
	{ 13, "Env 1 Delay" },
	{ 14, "Env 1 Attack" },
	{ 15, "Env 1 Decay" },
	{ 16, "Env 1 Release" },
	{ 17, "Env 1 Amplitude" },
	{ 18, "Env 2 Delay" },
	{ 19, "Env 2 Attack" },
	{ 20, "Env 2 Decay" },
	{ 21, "Env 2 Release" },
	{ 22, "Env 2 Amplitude" },
	{ 23, "Env 3 Delay" },
	{ 24, "Env 3 Attack" },
	{ 25, "Env 3 Decay" },
	{ 26, "Env 3 Release" },
	{ 27, "Env 3 Amplitude" },
	{ 28, "LFO 1 Speed" },
	{ 29, "LFO 1 Amplitude" },
	{ 30, "LFO 2 Speed" },
	{ 31, "LFO 2 Amplitude" },
	{ 32, "Portamento Time" },
};

// Regarding https://www.untergeek.de/howto/oberheim-matrix-1000/oberheim-matrix-1000-firmware-v1-20/,
// these are the realtime varying parameters in the 1.20 Bob Grieb firmware.
std::set<int> fastParameters = {
	1, 3, 4, 7, 9, 11, 13, 14, 17, 19, 21, 22, 23, 24, 25, 27, 28, 30, 31, 32
};


TActivePredicate cPortamentoEnabled = [](Patch const &patch) { return (patch.at(29) & 0x1) != 0; };

std::vector<int> modSourceIndexes = { 104, 107, 110, 113, 116, 119, 122, 125, 128, 131 };

TActivePredicate cTrackingUsed = [](Patch const &patch) { return std::any_of(modSourceIndexes.begin(), modSourceIndexes.end(), [&](int sourceIndex) { return patch.at(sourceIndex) == 11; }); };
TActivePredicate cRamp1Used = [](Patch const &patch) { return std::any_of(modSourceIndexes.begin(), modSourceIndexes.end(), [&](int sourceIndex) { return patch.at(sourceIndex) == 7; }); };
TActivePredicate cRamp2Used = [](Patch const &patch) { return std::any_of(modSourceIndexes.begin(), modSourceIndexes.end(), [&](int sourceIndex) { return patch.at(sourceIndex) == 8; }); };

std::vector<SynthParameterDefinition *> Matrix1000ParamDefinition::allDefinitions = {
	new Matrix1000ParamDefinition(Keyboard_mode,8, 	48, 	2,	"Keyboard mode",{ { 0 , "Reassign" },{ 1 , "Rotate" },{ 2 , "Unison" },{ 3 , "Reassign w / Rob" } }),
	new Matrix1000ParamDefinition(DCO_1_Initial_Frequency_LSB,9, 	00, 	6, 	"DCO 1 Initial Frequency  LSB = 1 Semitone"),
	new Matrix1000ParamDefinition(DCO_1_Initial_Waveshape_0,10, 	05, 	6, 	"DCO 1 Initial Waveshape  0 = Sawtooth  31 = Triangle", [](Patch const &patch) { return (patch.at(13) & 0x2) != 0; }),
	new Matrix1000ParamDefinition(DCO_1_Initial_Pulse_width,11, 	03, 	6, 	"DCO 1 Initial Pulse width", [](Patch const &patch) { return (patch.at(13) & 0x1) != 0; }),
	new Matrix1000ParamDefinition(DCO_1_Fixed_Modulations_PitchBend,12 ,	07, 	2, 	0, "DCO 1 Fixed Modulations  Bit0 = Lever 1", true),
	new Matrix1000ParamDefinition(DCO_1_Fixed_Modulations_Vibrato,12 ,	07, 	2, 	1, "DCO 1 Fixed Modulations  Bit1 = Vibrato", true),
	new Matrix1000ParamDefinition(DCO_1_Waveform_Enable_Pulse,13, 	06, 	2, 	0, "DCO 1 Waveform Enable  Bit0 = Pulse", true),
	new Matrix1000ParamDefinition(DCO_1_Waveform_Enable_Saw,13, 	06, 	2, 	1, "DCO 1 Waveform Enable  Bit1 = Wave", true),
	new Matrix1000ParamDefinition(DCO_2_Initial_Frequency_LSB,14, 	10, 	6, 	"DCO 2 Initial Frequency  LSB = 1 Semitone"),
	new Matrix1000ParamDefinition(DCO_2_Initial_Waveshape_0,15, 	15, 	6, 	"DCO 2 Initial Waveshape  0 = Sawtooth  31 = Triangle", [](Patch const &patch) { return (patch.at(18) & 0x2) != 0; }),
	new Matrix1000ParamDefinition(DCO_2_Initial_Pulse_width,16, 	13, 	6, 	"DCO 2 Initial Pulse width", [](Patch const &patch) { return (patch.at(18) & 0x1) != 0; }),
	new Matrix1000ParamDefinition(DCO_2_Fixed_Modulations_PitchBend,17, 	17, 	2,  0, "DCO 2 Fixed Modulations  Bit0 = Lever 1", true),
	new Matrix1000ParamDefinition(DCO_2_Fixed_Modulations_Vibrato,17, 	17, 	2, 	1, "DCO 2 Fixed Modulations  Bit1 = Vibrato", true),
	new Matrix1000ParamDefinition(DCO_2_Waveform_Enable_Pulse,18, 	16, 	3, 	0, "DCO 2 Waveform Enable  Bit0 = Pulse", true),
	new Matrix1000ParamDefinition(DCO_2_Waveform_Enable_Saw,18, 	16, 	3, 	1, "DCO 2 Waveform Enable  Bit1 = Wave", true),
	new Matrix1000ParamDefinition(DCO_2_Waveform_Enable_Noise,18, 	16, 	3, 	2, "DCO 2 Waveform Enable  Bit2 = Noise", true),
	new Matrix1000ParamDefinition(DCO_2_Detune,19, 	12, 	-6 /* (signed) */, "DCO 2 Detune", [](Patch const &patch) { return (patch.at(18) & 0x3) != 0; }),
	new Matrix1000ParamDefinition(MIX,20, 	20, 	6, 	"Mix"),
	new Matrix1000ParamDefinition(DCO_1_Fixed_Modulations_Portamento,21,	 8,	/*2*/ 1, 0, "DCO 1 Fixed Modulations  Bit0 = Portamento  (Bit1 = Not used)", true),
	new Matrix1000ParamDefinition(DCO_1_Click,22,	 9,	1, 0, "DCO 1 Click", true),
	new Matrix1000ParamDefinition(DCO_2_Fixed_Modulations_Portamento,23,	18,	2, 0, "DCO 2 Fixed Modulations  Bit0 = Portamento", true),
	new Matrix1000ParamDefinition(DCO_2_Fixed_Modulations_KeyboardTracking,23,	18,	2, 1, "DCO 2 Fixed Modulations  Bit1 = Keyboard Tracking enable", true),
	new Matrix1000ParamDefinition(DCO_2_Click,24,	19,	1, 0, "DCO 2 Click", true),
	new Matrix1000ParamDefinition(DCO_Sync_Mode,25,	02,	2,"DCO Sync mode",{ { 0, "NO" },{ 1,"SOFT" },{ 2, "MEDIUM" },{ 3, "HARD" } }, [](Patch const &patch) { return patch.at(25) != 0; }),
	new Matrix1000ParamDefinition(VCF_Initial_Frequency_LSB,26,	21,	7,"VCF Initial Frequency  LSB = 1 Semitone"),
	new Matrix1000ParamDefinition(VCF_Initial_Resonance,27,	24,	6,"VCF Initial Resonance"),
	new Matrix1000ParamDefinition(VCF_Fixed_Modulations_Lever1,28,	25,	2, 0, "VCF Fixed Modulations  Bit0 = Lever 1", true),
	new Matrix1000ParamDefinition(VCF_Fixed_Modulations_Vibrato,28,	25,	2, 1, "VCF Fixed Modulations  Bit1 = Vibrato", true),
	new Matrix1000ParamDefinition(VCF_Keyboard_Modulation_Portamento,29,	26,	2, 0, "VCF Keyboard Modulation  Bit0 = Portamento", true),
	new Matrix1000ParamDefinition(VCF_Keyboard_Modulation_Key,29,	26,	2, 1, "VCF Keyboard Modulation  Bit1 = Keyboard", true),
	new Matrix1000ParamDefinition(VCF_FM_Initial_Amount,30,	30,	6, "VCF FM Initial Amount"),
	new Matrix1000ParamDefinition(VCA_1_exponential_Initial_Amount,31,	27,	6, "VCA 1 (exponential)Initial Amount"),
	new Matrix1000ParamDefinition(Portamento_Initial_Rate,32,	44,	6,"Portamento Initial Rate", cPortamentoEnabled),
	new Matrix1000ParamDefinition(Exponential_2,33,	46,	2,"Lag Mode",{ { 0, "Constant Speed" },{ 1, "Constant Time" },{ 2, "Exponential 1" },{ 3, "Exponential 2" } }, cPortamentoEnabled),
	new Matrix1000ParamDefinition(Legato_Portamento_Enable,34,	47,	1,"Legato Portamento Enable", cPortamentoEnabled),
	new Matrix1000ParamDefinition(LFO_1_Initial_Speed,35,	80,	6,"LFO 1 Initial Speed"),
	new Matrix1000ParamDefinition(LFO_1_Trigger,36,	86,	2 , "LFO 1 Trigger",{ { 0, "No Trigger" },{ 1, "Single Trigger" },{ 2, "Multi Trigger" },{ 3, "External Trigger" } }),
	new Matrix1000ParamDefinition(LFO_1_Lag_Enable,37,	87,	1 , "LFO 1 Lag Enable"),
	new Matrix1000ParamDefinition(LFO_1_Waveshape,38,	82,	3 , "LFO 1 Waveshape(see table 1)", lfoWaveCodes),
	new Matrix1000ParamDefinition(LFO_1_Retrigger_point,39,	83,	5 , "LFO 1 Retrigger point"),
	new Matrix1000ParamDefinition(LFO_1_Sampled_Source_Number,40,	88,	5 , "LFO 1 Sampled Source Number"),
	new Matrix1000ParamDefinition(LFO_1_Initial_Amplitude,41,	84,	6 , "LFO 1 Initial Amplitude"),
	new Matrix1000ParamDefinition(LFO_2_Initial_Speed,42,	90,	6 , "LFO 2 Initial Speed"),
	new Matrix1000ParamDefinition(LFO_2_Trigger,43,	96,	2 , "LFO 2 Trigger",{ { 0, "No Trigger" },{ 1, "Single Trigger" },{ 2, "Multi Trigger" },{ 3, "External Trigger" } }),
	new Matrix1000ParamDefinition(LFO_2_Lag_Enable,44,	97,	1 , "LFO 2 Lag Enable"),
	new Matrix1000ParamDefinition(LFO_2_Waveshape,45,	92,	3 , "LFO 2 Waveshape", lfoWaveCodes),
	new Matrix1000ParamDefinition(LFO_2_Retrigger_point,46,	93,	5 , "LFO 2 Retrigger point"),
	new Matrix1000ParamDefinition(LFO_2_Sampled_Source_Number,47,	98,	5 , "LFO 2 Sampled Source Number"),
	new Matrix1000ParamDefinition(LFO_2_Initial_Amplitude,48,	94,	6 , "LFO 2 Initial Amplitude"),
	new Matrix1000ParamDefinition(Env_1_Trigger_Mode_Bit0,49,	57,	3, 0, "Env 1 Trigger Mode  Bit0 = Reset", true),
	new Matrix1000ParamDefinition(Env_1_Trigger_Mode_Bit1,49,	57,	3, 1, "Env 1 Trigger Mode  Bit1 = Multi Trigger", true),
	new Matrix1000ParamDefinition(Env_1_Trigger_Mode_Bit2,49,	57,	3, 2, "Env 1 Trigger Mode  Bit2 = External Trigger", true),
	new Matrix1000ParamDefinition(Env_1_Initial_Delay_Time,50,	50,	6, "Env 1 Initial Delay Time"),
	new Matrix1000ParamDefinition(Env_1_Initial_Attack_Time,51,	51,	6, "Env 1 Initial Attack Time"),
	new Matrix1000ParamDefinition(Env_1_Initial_Decay_Time,52,	52,	6, "Env 1 Initial Decay Time"),
	new Matrix1000ParamDefinition(Env_1_Sustain_Level,53,	53,	6, "Env 1 Sustain Level"),
	new Matrix1000ParamDefinition(Env_1_Initial_Release_Time,54,	54,	6, "Env 1 Initial Release Time"),
	new Matrix1000ParamDefinition(Env_1_Initial_Amplitude,55,	55,	6, "Env 1 Initial Amplitude"),
	new Matrix1000ParamDefinition(Env_1_LFO_Trigger_Mode_Bit0,56,	59,	2, 0, "Env 1 LFO Trigger Mode  Bit0 = Gated", true),
	new Matrix1000ParamDefinition(Env_1_LFO_Trigger_Mode_Bit1,56,	59,	2, 1, "Env 1 LFO Trigger Mode  Bit1 = LFO Trigger", true),
	new Matrix1000ParamDefinition(Env_1_Mode_Bit0,57,	58,	2, 0, "Env 1 Mode  Bit0 = DADR Mode", true),
	new Matrix1000ParamDefinition(Env_1_Mode_Bit1,57,	58,	2, 1, "Env 1 Mode  Bit1 = Freerun", true),
	new Matrix1000ParamDefinition(Env_2_Trigger_Mode_Bit0,58, 67,	3, 0, "Env 2 Trigger Mode  Bit0 = Reset", true),
	new Matrix1000ParamDefinition(Env_2_Trigger_Mode_Bit1,58, 67,	3, 1, "Env 2 Trigger Mode  Bit1 = Multi Trigger", true),
	new Matrix1000ParamDefinition(Env_2_Trigger_Mode_Bit2,58, 67,	3, 2, "Env 2 Trigger Mode  Bit2 = External Trigger", true),
	new Matrix1000ParamDefinition(Env_2_Initial_Delay_Time,59,	60,	6,"Env 2 Initial Delay Time"),
	new Matrix1000ParamDefinition(Env_2_Initial_Attack_Time,60,	61,	6,"Env 2 Initial Attack Time"),
	new Matrix1000ParamDefinition(Env_2_Initial_Decay_Time,61,	62,	6,"Env 2 Initial Decay Time"),
	new Matrix1000ParamDefinition(Env_2_Sustain_Level,62,	63,	6,"Env 2 Sustain Level"),
	new Matrix1000ParamDefinition(Env_2_Initial_Release_Time,63,	64,	6,"Env 2 Initial Release Time"),
	new Matrix1000ParamDefinition(Env_2_Initial_Amplitude,64,	65,	6,"Env 2 Initial Amplitude"),
	new Matrix1000ParamDefinition(Env_2_LFO_Trigger_Mode_Bit0,65,	69,	2, 0, "Env 2 LFO Trigger Mode  Bit0 = Gated", true),
	new Matrix1000ParamDefinition(Env_2_LFO_Trigger_Mode_Bit1,65,	69,	2, 1, "Env 2 LFO Trigger Mode  Bit1 = LFO Trigger", true),
	new Matrix1000ParamDefinition(Env_2_Mode_Bit0,66,	68,	2, 0, "Env 2 Mode  Bit0 = DADR Mode", true),
	new Matrix1000ParamDefinition(Env_2_Mode_Bit1,66,	68,	2, 1, "Env 2 Mode  Bit1 = Freerun", true),
	new Matrix1000ParamDefinition(Env_3_Trigger_Mode_Bit0,67,	77,	3, 0, "Env 3 Trigger Mode  Bit0 = Reset", true),
	new Matrix1000ParamDefinition(Env_3_Trigger_Mode_Bit1,67,	77,	3, 1, "Env 3 Trigger Mode  Bit1 = Multi Trigger", true),
	new Matrix1000ParamDefinition(Env_3_Trigger_Mode_Bit2,67,	77,	3, 2, "Env 3 Trigger Mode  Bit2 = External Trigger", true),
	new Matrix1000ParamDefinition(Env_3_Initial_Delay_Time,68,	70,	6,"Env 3 Initial Delay Time"),
	new Matrix1000ParamDefinition(Env_3_Initial_Attack_Time,69,	71,	6,"Env 3 Initial Attack Time"), // Erratum: This is wrong in the documentation, which says 69 as the parameter number.
	new Matrix1000ParamDefinition(Env_3_Initial_Decay_Time,70,	72,	6,"Env 3 Initial Decay Time"),
	new Matrix1000ParamDefinition(Env_3_Sustain_Level,71,	73,	6,"Env 3 Sustain Level"),
	new Matrix1000ParamDefinition(Env_3_Initial_Release_Time,72,	74,	6,"Env 3 Initial Release Time"),
	new Matrix1000ParamDefinition(Env_3_Initial_Amplitude,73,	75,	6,"Env 3 Initial Amplitude"),
	new Matrix1000ParamDefinition(Env_3_LFO_Trigger_Mode_Bit0,74,	79,	2, 0, "Env 3 LFO Trigger Mode  Bit0 = Gated", true),
	new Matrix1000ParamDefinition(Env_3_LFO_Trigger_Mode_Bit1,74,	79,	2, 1, "Env 3 LFO Trigger Mode  Bit1 = LFO Trigger", true),
	new Matrix1000ParamDefinition(Env_3_Mode_Bit0,75,	78,	2, 0, "Env 3 Mode  Bit0 = DADR Mode", true),
	new Matrix1000ParamDefinition(Env_3_Mode_Bit1,75,	78,	2, 1, "Env 3 Mode  Bit1 = Freerun", true),
	new Matrix1000ParamDefinition(Tracking_Generator_Input_Source_Code,	76, 33,	5,	"Tracking Generator Input Source Code(See Table 2)", modulationSourceCodes, cTrackingUsed),
	new Matrix1000ParamDefinition(Tracking_Point_1,77,	34,	6,	"Tracking Point 1", cTrackingUsed),
	new Matrix1000ParamDefinition(Tracking_Point_2,78,	35,	6,	"Tracking Point 2", cTrackingUsed),
	new Matrix1000ParamDefinition(Tracking_Point_3,79,	36,	6,	"Tracking Point 3", cTrackingUsed),
	new Matrix1000ParamDefinition(Tracking_Point_4,80,	37,	6,	"Tracking Point 4", cTrackingUsed),
	new Matrix1000ParamDefinition(Tracking_Point_5,81,	38,	6,	"Tracking Point 5", cTrackingUsed),
	new Matrix1000ParamDefinition(Ramp_1_Rate,82,	40,	6,	"Ramp 1 Rate", cRamp1Used),
	new Matrix1000ParamDefinition(Ramp1_Mode,83,	41,	2,	"Ramp 1 Mode",{ { 0, "Single Trigger" },{ 1, "Multi Trigger" },{ 2, "External Trigger" },{ 3, "External Gated" } }, cRamp1Used),
	new Matrix1000ParamDefinition(Ramp2_Rate,84,	42,	6,	"Ramp 2 Rate", cRamp2Used),
	new Matrix1000ParamDefinition(Ramp2_Mode,85,	43,	2,	"Ramp 2 Mode",{ { 0, "Single Trigger" },{ 1, "Multi Trigger" },{ 2, "External Trigger" },{ 3, "External Gated" } }, cRamp2Used),
	new Matrix1000ParamDefinition(DCO_1_Freq_by_LFO_1_Amount,86,	01,	-7, "DCO 1 Freq.by LFO 1 Amount", [](Patch const &patch) { return patch.value(patch.paramBySysexIndex(86)) != 0; }),
	new Matrix1000ParamDefinition(DCO_1_PW_by_LFO_2_Amount,87,	04,	-7, "DCO 1 PW by LFO 2 Amount", [](Patch const &patch) { return patch.value(patch.paramBySysexIndex(87)) != 0 && (patch.value(patch.paramBySysexIndex(13)) & 0x02); }),
	new Matrix1000ParamDefinition(DCO_2_Freq_by_LFO_1_Amount,88,	11,	-7, "DCO 2 Freq.by LFO 1 Amount", [](Patch const &patch) { return patch.value(patch.paramBySysexIndex(88)) != 0; }),
	new Matrix1000ParamDefinition(DCO_2_PW_by_LFO_2_Amount,	89,	14,	-7, "DCO 2 PW by LFO 2 Amount", [](Patch const &patch) { return patch.value(patch.paramBySysexIndex(89)) != 0; }),
	new Matrix1000ParamDefinition(VCF_Freq_by_Env_1_Amount,90,	22,	-7, "VCF Freq.by Env 1 Amount", [](Patch const &patch) { return patch.value(patch.paramBySysexIndex(90)) != 0; }),
	new Matrix1000ParamDefinition(VCF_Freq_by_Pressure_Amount,91,	23,	-7, "VCF Freq.by Pressure Amount", [](Patch const &patch) { return patch.value(patch.paramBySysexIndex(91)) != 0; }),
	new Matrix1000ParamDefinition(VCA_1_by_Velocity_Amount,	92,	28,	-7, "VCA 1 by Velocity Amount", [](Patch const &patch) { return patch.value(patch.paramBySysexIndex(92)) != 0; }),
	new Matrix1000ParamDefinition(VCA_2_by_Env_2_Amount,93,	29,	-7, "VCA 2 by Env 2 Amount", [](Patch const &patch) { return patch.value(patch.paramBySysexIndex(93)) != 0; }),
	new Matrix1000ParamDefinition(Env_1_Amplitude_by_Velocity_Amount,94,	56,	-7, "Env 1 Amplitude by Velovity Amount", [](Patch const &patch) { return patch.value(patch.paramBySysexIndex(94)) != 0; }),
	new Matrix1000ParamDefinition(Env_2_Amplitude_by_Velocity_Amount,95,	66,	-7, "Env 2 Amplitude by Velovity Amount", [](Patch const &patch) { return patch.value(patch.paramBySysexIndex(95)) != 0; }),
	new Matrix1000ParamDefinition(Env_3_Amplitude_by_Velocity_Amount,96,	76,	-7, "Env 3 Amplitude by Velovity Amount", [](Patch const &patch) { return patch.value(patch.paramBySysexIndex(96)) != 0; }),
	new Matrix1000ParamDefinition(LFO_1_Amp_by_Ramp_1_Amount,97,	85,	-7, "LFO 1 Amp.by Ramp 1 Amount", [](Patch const &patch) { return patch.value(patch.paramBySysexIndex(97)) != 0; }),
	new Matrix1000ParamDefinition(LFO_2_Amp_by_Ramp_2_Amount,98,	95,	-7, "LFO 2 Amp.by Ramp 2 Amount", [](Patch const &patch) { return patch.value(patch.paramBySysexIndex(98)) != 0; }),
	new Matrix1000ParamDefinition(Portamento_rate_by_Velocity_Amount,99,	45,	-7, "Portamento rate by Velocity Amount", cPortamentoEnabled),
	new Matrix1000ParamDefinition(VCF_FM_Amount_by_Env_3_Amount,100,	31,	-7, "VCF FM Amount by Env 3 Amount", [](Patch const &patch) { return patch.value(patch.paramBySysexIndex(100)) != 0; }),
	new Matrix1000ParamDefinition(VCF_FM_Amount_by_Pressure_Amount,101,	32,	-7, "VCF FM Amount by Pressure Amount", [](Patch const &patch) { return patch.value(patch.paramBySysexIndex(101)) != 0; }),
	new Matrix1000ParamDefinition(LFO_1_Speed_by_Pressure_Amount,102,	81,	-7, "LFO 1 Speed by Pressure Amount", [](Patch const &patch) { return patch.value(patch.paramBySysexIndex(102)) != 0; }),
	new Matrix1000ParamDefinition(LFO_2_Speed_by_Keyboard_Amount,103,	91,	-7, "LFO 2 Speed by Keyboard Amount", [](Patch const &patch) { return patch.value(patch.paramBySysexIndex(103)) != 0; }),
	new Matrix1000ParamDefinition(Matrix_Modulation_Bus_0_Source_Code,104,	  	5,	"Matrix Modulation Bus 0 Source Code", modulationSourceCodes, [](Patch const &data) { return data.at(104) != 0 && data.at(106) != 0;  }),
	new Matrix1000ParamDefinition(M_Bus_0_Amount,105, -7, "M Bus 0 Amount", [](Patch const &data) { return data.at(104) != 0 && data.at(106) != 0;  }),
	new Matrix1000ParamDefinition(MM_Bus_0_Destination_Code,106,	  	5,	"MM Bus 0 Destination Code", modulationDestinationCodes, [](Patch const &data) { return data.at(104) != 0 && data.at(106) != 0;  }),
	new Matrix1000ParamDefinition(Matrix_Modulation_Bus_1_Source_Code,107,	  	5,	"Matrix Modulation Bus 1 Source Code", modulationSourceCodes, [](Patch const &data) { return data.at(107) != 0 && data.at(109) != 0;  }),
	new Matrix1000ParamDefinition(M_Bus_1_Amount,108,	  	-7, "M Bus 1 Amount", [](Patch const &data) { return data.at(107) != 0 && data.at(109) != 0;  }),
	new Matrix1000ParamDefinition(MM_Bus_1_Destination_Code,109,	  	5,	"MM Bus 1 Destination Code", modulationDestinationCodes, [](Patch const &data) { return data.at(107) != 0 && data.at(109) != 0;  }),
	new Matrix1000ParamDefinition(Matrix_Modulation_Bus_2_Source_Code,110,	  	5,	"Matrix Modulation Bus 2 Source Code", modulationSourceCodes, [](Patch const &data) { return data.at(110) != 0 && data.at(112) != 0;  }),
	new Matrix1000ParamDefinition(M_Bus_2_Amount,111,	  	-7, "M Bus 2 Amount", [](Patch const &data) { return data.at(110) != 0 && data.at(112) != 0;  }),
	new Matrix1000ParamDefinition(MM_Bus_2_Destination_Code,112,	  	5,	"MM Bus 2 Destination Code", modulationDestinationCodes, [](Patch const &data) { return data.at(110) != 0 && data.at(112) != 0;  }),
	new Matrix1000ParamDefinition(Matrix_Modulation_Bus_3_Source_Code,113,	  	5,	"Matrix Modulation Bus 3 Source Code", modulationSourceCodes, [](Patch const &data) { return data.at(113) != 0 && data.at(115) != 0;  }),
	new Matrix1000ParamDefinition(M_Bus_3_Amount,114,	  	-7, "M Bus 3 Amount", [](Patch const &data) { return data.at(113) != 0 && data.at(115) != 0;  }),
	new Matrix1000ParamDefinition(MM_Bus_3_Destination_Code,115,	  	5,	"MM Bus 3 Destination Code", modulationDestinationCodes, [](Patch const &data) { return data.at(113) != 0 && data.at(115) != 0;  }),
	new Matrix1000ParamDefinition(Matrix_Modulation_Bus_4_Source_Code,116,	  	5,	"Matrix Modulation Bus 4 Source Code", modulationSourceCodes, [](Patch const &data) { return data.at(116) != 0 && data.at(118) != 0;  }),
	new Matrix1000ParamDefinition(M_Bus_4_Amount,117,	  	-7, "M Bus 4 Amount", [](Patch const &data) { return data.at(116) != 0 && data.at(118) != 0;  }),
	new Matrix1000ParamDefinition(MM_Bus_4_Destination_Code,118,	  	5,	"MM Bus 4 Destination Code", modulationDestinationCodes, [](Patch const &data) { return data.at(116) != 0 && data.at(118) != 0;  }),
	new Matrix1000ParamDefinition(Matrix_Modulation_Bus_5_Source_Code,119,	  	5,	"Matrix Modulation Bus 5 Source Code", modulationSourceCodes, [](Patch const &data) { return data.at(119) != 0 && data.at(121) != 0;  }),
	new Matrix1000ParamDefinition(M_Bus_5_Amount,120,	  	-7, "M Bus 5 Amount", [](Patch const &data) { return data.at(119) != 0 && data.at(121) != 0;  }),
	new Matrix1000ParamDefinition(MM_Bus_5_Destination_Code,121,	  	5,	"MM Bus 5 Destination Code", modulationDestinationCodes, [](Patch const &data) { return data.at(119) != 0 && data.at(121) != 0;  }),
	new Matrix1000ParamDefinition(Matrix_Modulation_Bus_6_Source_Code,122,	  	5,	"Matrix Modulation Bus 6 Source Code", modulationSourceCodes, [](Patch const &data) { return data.at(122) != 0 && data.at(124) != 0;  }),
	new Matrix1000ParamDefinition(M_Bus_6_Amount,123,	  	-7, "M Bus 6 Amount", [](Patch const &data) { return data.at(122) != 0 && data.at(124) != 0;  }),
	new Matrix1000ParamDefinition(MM_Bus_6_Destination_Code,124,	  	5,	"MM Bus 6 Destination Code", modulationDestinationCodes, [](Patch const &data) { return data.at(122) != 0 && data.at(124) != 0;  }),
	new Matrix1000ParamDefinition(Matrix_Modulation_Bus_7_Source_Code,125,	  	5,	"Matrix Modulation Bus 7 Source Code", modulationSourceCodes, [](Patch const &data) { return data.at(125) != 0 && data.at(127) != 0;  }),
	new Matrix1000ParamDefinition(M_Bus_7_Amount,126,	  	-7, "M Bus 7 Amount", [](Patch const &data) { return data.at(125) != 0 && data.at(127) != 0;  }),
	new Matrix1000ParamDefinition(MM_Bus_7_Destination_Code,127,	  	5,	"MM Bus 7 Destination Code", modulationDestinationCodes, [](Patch const &data) { return data.at(125) != 0 && data.at(127) != 0;  }),
	new Matrix1000ParamDefinition(Matrix_Modulation_Bus_8_Source_Code,128,	  	5,	"Matrix Modulation Bus 8 Source Code", modulationSourceCodes, [](Patch const &data) { return data.at(128) != 0 && data.at(130) != 0;  }),
	new Matrix1000ParamDefinition(M_Bus_8_Amount,129,	  	-7, "M Bus 8 Amount", [](Patch const &data) { return data.at(128) != 0 && data.at(130) != 0;  }),
	new Matrix1000ParamDefinition(MM_Bus_8_Destination_Code,130,	  	5,	"MM Bus 8 Destination Code", modulationDestinationCodes, [](Patch const &data) { return data.at(128) != 0 && data.at(130) != 0;  }),
	new Matrix1000ParamDefinition(Matrix_Modulation_Bus_9_Source_Code,131,	  	5,	"Matrix Modulation Bus 9 Source Code", modulationSourceCodes, [](Patch const &data) { return data.at(131) != 0 && data.at(133) != 0;  }),
	new Matrix1000ParamDefinition(M_Bus_9_Amount,132,	  	-7, "M Bus 9 Amount", [](Patch const &data) { return data.at(131) != 0 && data.at(133) != 0;  }),
	new Matrix1000ParamDefinition(MM_Bus_9_Destination_Code, 133,	  	5,	"MM Bus 9 Destination Code", modulationDestinationCodes, [](Patch const &data) { return data.at(131) != 0 && data.at(133) != 0;  })
};

// Unison detune can be controlled via MIDI CC #94