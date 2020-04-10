/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "SynthParameterDefinition.h"

namespace midikraft {

	// Definitions from the manual, see p. 49ff
	const uint8 ROLAND_ID = 0b01000001;
	const uint8 MKS80_ID = 0b00100000;

	// These are identical with the MKS50!
	enum class MKS80_Operation_Code {
		INVALID = 0b00000000, // For error signaling
		APR = 0b00110101, // All parameters
		//BLD = 0b00110111, // Bulk dump - the MKS80 doesn't seem to have that capability!
		PGR = 0b00110100, // PGR, MKS-80 only, not on MKS-50
		IPR = 0b00110110, // Individual parameter
		WSF = 0b01000000, // Want to send file
		RQF = 0b01000001, // Request file
		DAT = 0b01000010, // Data
		ACK = 0b01000011, // Acknowledge
		EOF_ = 0b01000101, // End of file [Who defined EOF as a macro??]
		ERR = 0b01001110, // Error
		RJC = 0b01001111, // Rejection
	};

	class MKS80_Parameter : public SynthParameterDefinition, public SynthIntParameterCapability {
	public:
		enum ParameterType {
			TONE,
			PATCH
		};
		enum SynthSection {
			LOWER = 0, // Numbers just to be able to iterate
			UPPER = 1
		};
		enum ToneParameter {
			LFO1_RATE = 0,
			LFO1_DELAY_TIME = 1,
			LFO1_WAVEFORM,
			VCO_MOD_LFO1_DEPTH,
			VCO_MOD_ENV1_DEPTH,
			PULSE_WIDTH,
			PULSE_WIDTH_MOD,
			PWM_MODE_SELECT,
			PWM_POLARITY,
			VCO_KEY_FOLLOW,
			VCO_SELECT,
			XMOD_MANUAL_DEPTH,
			XMOD_ENV1_DEPTH,
			XMOD_POLARITY,
			VCO1_MOD,
			VCO1_RANGE,
			VCO1_WAVEFORM,
			VCO_SYNC,
			VCO2_MOD,
			VCO2_RANGE,
			VCO_FINE_TUNE,
			VCO2_WAVEFORM,
			MIXER,
			HPF_CUTOFF_FREQ,
			VCF_CUTOFF_FREQ,
			VCF_RESONANCE,
			VCF_ENV_SELECT,
			VCF_ENV_POLARITY,
			VCF_MOD_ENV_DEPTH,
			VCF_MOD_LFO1_DEPTH,
			VCF_KEY_FOLLOW,
			VCA_ENV2,
			VCA_MOD_LFO1_DEPTH,
			DYNAMICS_TIME,
			DYNAMICS_LEVEL,
			ENV_RESET,
			ENV1_DYNAMICS,
			ENV1_ATTACK,
			ENV1_DECAY,
			ENV1_SUSTAIN,
			ENV1_RELEASE,
			ENV1_KEY_FOLLOW,
			ENV2_DYNAMICS,
			ENV2_ATTACK,
			ENV2_DECAY,
			ENV2_SUSTAIN,
			ENV2_RELEASE,
			ENV2_KEY_FOLLOW
		};
		enum PatchParameter {
			KEY_MODE_SELECT,
			SPLIT_POINT,
			BALANCE,
			TONE_NUMBER,
			OCTAVE_SHIFT,
			ASSIGN_MODE_SELECT,
			UNISON_DETUNE,
			HOLD,
			GLIDE,
			BENDER_SENSIVITY,
			VCO1_BEND,
			VCO2_BEND,
			AFTERTOUCH_SENSIVITY,
			AFTERTOUCH_MODE_SELECT,
			LFO2_RATE
		};

		MKS80_Parameter(ParameterType paramType, int paramIndex, std::string const &name, int min, int max);
		MKS80_Parameter(ParameterType paramType, int paramIndex, std::string const &name, int min, int max, std::map<int, std::string> const &valueLookup);

		ParamType type() const override;
		virtual std::string name() const override;
		
		std::string valueInPatchToText(Patch const &patch) const override;
		virtual std::string description() const override;

		// SynthIntParameterCapability
		virtual int sysexIndex() const override;
		virtual int minValue() const override;
		virtual int maxValue() const override;
		virtual bool valueInPatch(Patch const &patch, int &outValue) const override;
		virtual void setInPatch(Patch &patch, int value) const override;

		void setSection(SynthSection section);

		// Accessors to MKS80 specific data
		int paramIndex() const;
		ParameterType parameterType() const;
		SynthSection section() const;

		static std::vector<std::shared_ptr<SynthParameterDefinition>> allParameterDefinitions;
		static std::shared_ptr<MKS80_Parameter> findParameter(ParameterType type, SynthSection section, int parameterIndex);

	private:
		std::string valueAsText(int value) const;

		ParameterType paramType_;
		SynthSection section_; // This could be upper or lower
		int paramIndex_;
		std::string paramName_;
		int min_;
		int max_;
		std::map<int, std::string> valueLookup_;
	};

}
