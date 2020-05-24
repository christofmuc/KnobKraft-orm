/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "SynthParameterDefinition.h"

namespace midikraft {

	class KawaiK3Parameter : public SynthParameterDefinition, public SynthIntParameterCapability, SynthLookupParameterCapability {
	public:
		enum Parameter {
			OSC1_WAVE_SELECT = 1,
			OSC1_RANGE = 2,
			PORTAMENTO_SPEED = 3,
			OSC2_WAVE_SELECT = 7,
			OSC2_COARSE = 8,
			OSC2_FINE = 9,
			OSC_BALANCE = 4,
			OSC_AUTO_BEND = 6,
			MONO = -3,  // High bit in Param 5
			PITCH_BEND = 5,
			CUTOFF = 10,
			RESONANCE = 11,
			VCF_ENV = 13,
			VCF_ATTACK = 14,
			VCF_DECAY = 15,
			LOW_CUT = 12,
			VCF_SUSTAIN = 17,
			VCF_RELEASE = 18,
			VCA_LEVEL = 19,
			VCA_ATTACK = 20,
			VCA_DECAY = 21,
			VCA_SUSTAIN = 23,
			VCA_RELEASE = 24,
			LFO_SHAPE = 25,
			LFO_SPEED = 26,
			LFO_DELAY = 27,
			LFO_OSC = 28,
			LFO_VCF = 29,
			LFO_VCA = 30,
			VELOCITY_VCA = 32,
			VELOCITY_VCF = 31,
			PRESSURE_VCF = 34,
			PRESSURE_OSC_BALANCE = 33,
			PRESSURE_LFO_OSC = 36,
			PRESSURE_VCA = 35,
			KCV_VCF = 37,
			KCV_VCA = 38,
			CHORUS = 39,
			PORTAMENTO_SWITCH = -1, // No parameter number, not settable via sysex
			DEFAULT_PARAMETER = -2, // Part of sysex, but doesn't make sense as sysex
		};

		static std::vector<KawaiK3Parameter *> allParameters;
		static KawaiK3Parameter *findParameter(Parameter param);
		static int findWave(std::string shapename);
		static std::string waveName(int waveNo);

		KawaiK3Parameter(std::string const &name, Parameter param, int sysexIndex, int bits, int minValue, int maxValue);
		KawaiK3Parameter(std::string const &name, Parameter param, int sysexIndex, int bits, int shift, int minValue, int maxValue);

		// Basic SynthParameterDefinition
		virtual ParamType type() const override;
		virtual std::string name() const override;
		virtual std::string description() const override;
		virtual std::string valueInPatchToText(DataFile const &patch) const override;

		// SynthIntParameterCapability
		virtual int maxValue() const override;
		virtual int minValue() const override;
		virtual int sysexIndex() const override;
		virtual bool valueInPatch(DataFile const &patch, int &outValue) const override;
		virtual void setInPatch(DataFile &patch, int value) const override;
		
		// SynthLookupParameterCapability
		virtual std::string valueAsText(int value) const override;

		Parameter paramNo() const;
		int shift() const;
		int bits() const;
		int bitMask() const;
		int shiftedBitMask() const;

	private:
		std::string name_;
		Parameter paramNo_;
		int sysexIndex_;
		int sysexShift_;
		int sysexBits_;
		int minValue_;
		int maxValue_;
	};

}
