#pragma once

#include "SynthParameterDefinition.h"
#include "Synth.h"
#include "Patch.h"

typedef std::function<bool(Patch const &data)> TActivePredicate;

enum Matrix1000Param {
	Keyboard_mode,
	DCO_1_Initial_Frequency_LSB, //
	DCO_1_Initial_Waveshape_0, //
	DCO_1_Initial_Pulse_width,	  //
	DCO_1_Fixed_Modulations_PitchBend, //
	DCO_1_Fixed_Modulations_Vibrato, //
	DCO_1_Waveform_Enable_Pulse, //
	DCO_1_Waveform_Enable_Saw, //
	DCO_2_Initial_Frequency_LSB, //
	DCO_2_Initial_Waveshape_0, //
	DCO_2_Initial_Pulse_width, //
	DCO_2_Fixed_Modulations_PitchBend, //
	DCO_2_Fixed_Modulations_Vibrato, //
	DCO_2_Waveform_Enable_Pulse, //
	DCO_2_Waveform_Enable_Saw, //
	DCO_2_Waveform_Enable_Noise, //
	DCO_2_Detune, //
	MIX,
	DCO_1_Fixed_Modulations_Portamento, //
	DCO_1_Click, //
	DCO_2_Fixed_Modulations_Portamento, //
	DCO_2_Fixed_Modulations_KeyboardTracking, //
	DCO_2_Click, //
	DCO_Sync_Mode,
	VCF_Initial_Frequency_LSB,
	VCF_Initial_Resonance,
	VCF_Fixed_Modulations_Lever1, //
	VCF_Fixed_Modulations_Vibrato, //
	VCF_Keyboard_Modulation_Portamento, //
	VCF_Keyboard_Modulation_Key, //
	VCF_FM_Initial_Amount,
	VCA_1_exponential_Initial_Amount,
	Portamento_Initial_Rate,
	Exponential_2,
	Legato_Portamento_Enable,
	LFO_1_Initial_Speed, //
	LFO_1_Trigger,
	LFO_1_Lag_Enable,
	LFO_1_Waveshape, //
	LFO_1_Retrigger_point,
	LFO_1_Sampled_Source_Number,
	LFO_1_Initial_Amplitude, //
	LFO_2_Initial_Speed, //
	LFO_2_Trigger,
	LFO_2_Lag_Enable,
	LFO_2_Waveshape, //
	LFO_2_Retrigger_point,
	LFO_2_Sampled_Source_Number,
	LFO_2_Initial_Amplitude, //
	Env_1_Trigger_Mode_Bit0,
	Env_1_Trigger_Mode_Bit1,
	Env_1_Trigger_Mode_Bit2,
	Env_1_Initial_Delay_Time, //
	Env_1_Initial_Attack_Time, //
	Env_1_Initial_Decay_Time, //
	Env_1_Sustain_Level, //
	Env_1_Initial_Release_Time, //
	Env_1_Initial_Amplitude,
	Env_1_LFO_Trigger_Mode_Bit0,
	Env_1_LFO_Trigger_Mode_Bit1,
	Env_1_Mode_Bit0,
	Env_1_Mode_Bit1,
	Env_2_Trigger_Mode_Bit0,
	Env_2_Trigger_Mode_Bit1,
	Env_2_Trigger_Mode_Bit2,
	Env_2_Initial_Delay_Time, //
	Env_2_Initial_Attack_Time, //
	Env_2_Initial_Decay_Time, //
	Env_2_Sustain_Level, //
	Env_2_Initial_Release_Time, //
	Env_2_Initial_Amplitude,
	Env_2_LFO_Trigger_Mode_Bit0,
	Env_2_LFO_Trigger_Mode_Bit1,
	Env_2_Mode_Bit0,
	Env_2_Mode_Bit1,
	Env_3_Trigger_Mode_Bit0,
	Env_3_Trigger_Mode_Bit1,
	Env_3_Trigger_Mode_Bit2,
	Env_3_Initial_Delay_Time, //
	Env_3_Initial_Attack_Time, //
	Env_3_Initial_Decay_Time, //
	Env_3_Sustain_Level, //
	Env_3_Initial_Release_Time, //
	Env_3_Initial_Amplitude,
	Env_3_LFO_Trigger_Mode_Bit0,
	Env_3_LFO_Trigger_Mode_Bit1,
	Env_3_Mode_Bit0,
	Env_3_Mode_Bit1,
	Tracking_Generator_Input_Source_Code, //
	Tracking_Point_1,
	Tracking_Point_2,
	Tracking_Point_3,
	Tracking_Point_4,
	Tracking_Point_5,
	Ramp_1_Rate,
	Ramp1_Mode,
	Ramp2_Rate,
	Ramp2_Mode,
	DCO_1_Freq_by_LFO_1_Amount, //
	DCO_1_PW_by_LFO_2_Amount, //
	DCO_2_Freq_by_LFO_1_Amount, //
	DCO_2_PW_by_LFO_2_Amount, //
	VCF_Freq_by_Env_1_Amount, //
	VCF_Freq_by_Pressure_Amount, //
	VCA_1_by_Velocity_Amount, //
	VCA_2_by_Env_2_Amount, //
	Env_1_Amplitude_by_Velocity_Amount, //
	Env_2_Amplitude_by_Velocity_Amount, //
	Env_3_Amplitude_by_Velocity_Amount, //
	LFO_1_Amp_by_Ramp_1_Amount, //
	LFO_2_Amp_by_Ramp_2_Amount, //
	Portamento_rate_by_Velocity_Amount, //
	VCF_FM_Amount_by_Env_3_Amount, //
	VCF_FM_Amount_by_Pressure_Amount, //
	LFO_1_Speed_by_Pressure_Amount, //
	LFO_2_Speed_by_Keyboard_Amount, //
	Matrix_Modulation_Bus_0_Source_Code,
	M_Bus_0_Amount,
	MM_Bus_0_Destination_Code,
	Matrix_Modulation_Bus_1_Source_Code,
	M_Bus_1_Amount,
	MM_Bus_1_Destination_Code,
	Matrix_Modulation_Bus_2_Source_Code,
	M_Bus_2_Amount,
	MM_Bus_2_Destination_Code,
	Matrix_Modulation_Bus_3_Source_Code,
	M_Bus_3_Amount,
	MM_Bus_3_Destination_Code,
	Matrix_Modulation_Bus_4_Source_Code,
	M_Bus_4_Amount,
	MM_Bus_4_Destination_Code,
	Matrix_Modulation_Bus_5_Source_Code,
	M_Bus_5_Amount,
	MM_Bus_5_Destination_Code,
	Matrix_Modulation_Bus_6_Source_Code,
	M_Bus_6_Amount,
	MM_Bus_6_Destination_Code,
	Matrix_Modulation_Bus_7_Source_Code,
	M_Bus_7_Amount,
	MM_Bus_7_Destination_Code,
	Matrix_Modulation_Bus_8_Source_Code,
	M_Bus_8_Amount,
	MM_Bus_8_Destination_Code,
	Matrix_Modulation_Bus_9_Source_Code,
	M_Bus_9_Amount,
	MM_Bus_9_Destination_Code,

	// The following are not stored in the patch, and are not present in Sysex dumps!
	Volume,  
	GliGliDetune, // This is actually Unison Detune
	LAST
};

class Matrix1000ParamDefinition : public SynthParameterDefinition {
public:
	static std::vector<SynthParameterDefinition *> allDefinitions;

	Matrix1000ParamDefinition(Matrix1000Param id, int sysExIndex, int paramValue, int bits, std::string const &description) : 
		paramId_(id), sysexIndex_(sysExIndex), controller_(paramValue), bits_(bits), bitposition_(-1), description_(description), activeIfNonNull_(false) {
	}

	Matrix1000ParamDefinition(Matrix1000Param id, int sysExIndex, int paramValue, int bits, std::string const &description, TActivePredicate predicate) :
		paramId_(id), sysexIndex_(sysExIndex), controller_(paramValue), bits_(bits), bitposition_(-1), description_(description), activeIfNonNull_(false), testActive_(predicate) {
	}

	Matrix1000ParamDefinition(Matrix1000Param id, int sysExIndex, int paramValue, int bits, int bitposition, std::string const &description, bool activeIfNonNull = false) :
		paramId_(id), sysexIndex_(sysExIndex), controller_(paramValue), bits_(bits), bitposition_(bitposition), description_(description), activeIfNonNull_(activeIfNonNull) {
	}

	Matrix1000ParamDefinition(Matrix1000Param id, int sysExIndex, int paramValue, int bits, std::string const &description, TValueLookup const &valueLookup) :
		paramId_(id), sysexIndex_(sysExIndex), controller_(paramValue), bits_(bits), bitposition_(-1), description_(description), lookup_(valueLookup), activeIfNonNull_(false) {

	}

	Matrix1000ParamDefinition(Matrix1000Param id, int sysExIndex, int paramValue, int bits, std::string const &description, TValueLookup const &valueLookup, TActivePredicate predicate) :
		paramId_(id), sysexIndex_(sysExIndex), controller_(paramValue), bits_(bits), bitposition_(-1), description_(description), lookup_(valueLookup), activeIfNonNull_(false), testActive_(predicate) {

	}

	Matrix1000ParamDefinition(Matrix1000Param id, int sysExIndex, int bits, std::string const &description, TActivePredicate predicate) :
		paramId_(id), sysexIndex_(sysExIndex), controller_(-1), bits_(bits), bitposition_(-1), description_(description), activeIfNonNull_(false), testActive_(predicate) {
		// There are a few parameters that have no CC controller assigned, and can only be modified with specific sysex messages
	}

	Matrix1000ParamDefinition(Matrix1000Param id, int sysExIndex, int bits, std::string const &description, TValueLookup const &valueLookup, TActivePredicate predicate) :
		paramId_(id), sysexIndex_(sysExIndex), controller_(-1), bits_(bits), bitposition_(-1), description_(description), lookup_(valueLookup), activeIfNonNull_(false), testActive_(predicate) {
		// There are a few parameters that have no CC controller assigned, and can only be modified with specific sysex messages
	}

	Matrix1000Param id() const;
	int controller() const;
	int bits() const;
	int bitposition() const;

	virtual std::string name() const override;
	virtual std::string valueAsText(int value) const override;
	virtual std::string description() const override;
	virtual int sysexIndex() const override;
	virtual int minValue() const override;
	virtual int maxValue() const override;
	virtual bool matchesController(int controllerNumber) const override;

	virtual bool isActive(Patch const *patch) const override;

	virtual bool valueInPatch(Patch const &patch, int &outValue) const override;

	static SynthParameterDefinition const &param(Matrix1000Param id);

private:
	Matrix1000Param paramId_;
	int sysexIndex_;
	int controller_;
	int bits_;
	int bitposition_;
	bool activeIfNonNull_;
	std::string description_;
	TValueLookup lookup_;
	TActivePredicate testActive_;
};


