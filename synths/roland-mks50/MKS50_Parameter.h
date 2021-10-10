/*
   Copyright (c) 2021 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "SynthParameterDefinition.h"

namespace midikraft {

	class MKS50_Parameter : public SynthParameterDefinition, public SynthIntValueParameterCapability,
		public SynthLookupParameterCapability {
	public:
		enum ParameterType {
			TONE,
			PATCH,
			CHORD
		};
		enum ToneParameter {
			DCO_ENV_MODE,
			VCF_ENV_MODE,
			VCA_ENV_MODE,
			DCO_WAVEFORM_PULSE,
			DCO_WAVEFORM_SAWTOOTH,
			DCO_WAVEFORM_SUB,
			DCO_RANGE,
			DCO_SUB_LEVEL,
			DCO_NOISE_LEVEL,
			HPF_CUTOFF_FREQ,
			CHORUS,
			DCO_LFO_MOD_DEPTH,
			DCO_ENV_MOD_DEPTH,
			DCO_AFTER_DEPTH,
			DCO_PW_PWM_DEPTH,
			DCO_PWM_RATE,
			VCF_CUTOFF_FREQ,
			VCF_RESONANCE,
			VCF_LFO_MOD_DEPTH,
			VCF_ENV_MOD_DEPTH,
			VCF_KEY_FOLLOW,
			VCF_AFTER_DEPTH,
			VCA_LEVEL,
			VCA_AFTER_DEPTH,
			LFO_RATE,
			LFO_DELAY_TIME,
			ENV_T1,
			ENV_L1,
			ENV_T2,
			ENV_L2,
			ENV_T3,
			ENV_L3,
			ENV_T4,
			ENV_KEY_FOLLOW,
			CHORUS_RATE,
			BENDER_RANGE
		};
		enum PatchParameter {
			TONE_NUMBER,
			KEY_RANGE_LOW,
			KEY_RANGE_HIGH,
			PORTAMENTO_TIME,
			PORTAMENTO,
			MODULATION_SENSIVITY,
			KEY_SHIFT,
			VOLUME,
			DETUNE,
			MIDI_FUNCTION,
			MONO_BENDER_RATE,
			CHORD_MEMORY,
			KEY_ASSIGN_MODE
		};

		MKS50_Parameter(ParameterType paramType, int paramIndex, std::string const& name, int min, int max);
		MKS50_Parameter(ParameterType paramType, int paramIndex, std::string const& name, int min, int max, std::map<int, std::string> const& valueLookup);

		virtual std::string name() const override;
		ParamType type() const override;
		std::string valueInPatchToText(DataFile const& patch) const override;

		// SynthLookupParameterCapability
		virtual std::string valueAsText(int value) const override;

		virtual int sysexIndex() const override;
		virtual std::string description() const override;
		virtual int minValue() const override;
		virtual int maxValue() const override;

		static std::vector<std::shared_ptr<SynthParameterDefinition>> allParameterDefinitions;


	private:
		ParameterType paramType_;
		int paramIndex_;
		std::string paramName_;
		int min_;
		int max_;
		std::map<int, std::string> valueLookup_;
	};

}
