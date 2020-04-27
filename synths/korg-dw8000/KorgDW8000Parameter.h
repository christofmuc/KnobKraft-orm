/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "SynthParameterDefinition.h"

namespace midikraft {

	class KorgDW8000Parameter : public SynthParameterDefinition, public SynthIntParameterCapability {
	public:
		typedef std::map<int, std::string> TValueLookup;

		enum Parameter {
			OSC1_OCTAVE = 0,
			OSC1_WAVE_FORM = 1,
			OSC1_LEVEL = 2,
			AUTO_BEND_SELECT = 3,
			AUTO_BEND_MODE = 4,
			AUTO_BEND_TIME = 5,
			AUTO_BEND_INTENSITY = 6,
			OSC2_OCTAVE = 7,
			OSC2_WAVE_FORM = 8,
			OSC2_LEVEL = 9,
			INTERVAL = 10,
			DETUNE = 11,
			NOISE_LEVEL = 12,
			ASSIGN_MODE = 13, // This is misordered in the manual
			PARAMETER_NO_MEMORY = 14, // This is misordered in the manual
			CUTOFF = 15,
			RESONANCE = 16,
			KBD_TRACK = 17,
			POLARITY = 18,
			EG_INTENSITY = 19,
			VCF_ATTACK = 20,
			VCF_DECAY = 21,
			VCF_BREAK_POINT = 22,
			VCF_SLOPE = 23,
			VCF_SUSTAIN = 24,
			VCF_RELEASE = 25,
			VCF_VELOCITY_SENSIVITY = 26,
			VCA_ATTACK = 27,
			VCA_DECAY = 28,
			VCA_BREAK_POINT = 29,
			VCA_SLOPE = 30,
			VCA_SUSTAIN = 31,
			VCA_RELEASE = 32,
			VCA_VELOCITY_SENSIVITY = 33,
			MG_WAVE_FORM = 34,
			MG_FREQUENCY = 35,
			MG_DELAY = 36,
			MG_OSC = 37,
			MG_VCF = 38,
			BEND_OSC = 39, // Typo in manual which states 38, should be 39?
			BEND_VCF = 40,
			DELAY_TIME = 41,
			DELAY_FACTOR = 42,
			DELAY_FEEDBACK = 43,
			DELAY_FREQUENCY = 44,
			DELAY_INTENSITY = 45,
			DELAY_EFFECT_LEVEL = 46,
			PORTAMENTO = 47,
			AFTER_TOUCH_OSC_MG = 48,
			AFTER_TOUCH_VCF = 49,
			AFTER_TOUCH_VCA = 50
		};

		static std::vector<std::shared_ptr<SynthParameterDefinition>> allParameters;
		static std::shared_ptr <KorgDW8000Parameter> findParameter(Parameter param);

		KorgDW8000Parameter(Parameter const paramIndex, std::string const &name, int bits);
		KorgDW8000Parameter(Parameter const paramIndex, std::string const &name, int bits, uint8 maxValue);
		KorgDW8000Parameter(Parameter const paramIndex, std::string const &name, int bits, TValueLookup const &valueLookup);
		KorgDW8000Parameter(Parameter const paramIndex, std::string const &name, int bits, uint8 maxValue, TValueLookup const &valueLookup);

		// Implementation of interface
		virtual ParamType type() const override;
		virtual std::string name() const override;
		virtual std::string description() const override;
		virtual std::string valueInPatchToText(Patch const &patch) const override;

		// SynthIntParameterCapability
		virtual int minValue() const override;
		virtual int maxValue() const override;
		virtual int sysexIndex() const override;
		virtual bool valueInPatch(Patch const &patch, int &outValue) const override;
		virtual void setInPatch(Patch &patch, int value) const override;

		// old
		std::string valueAsText(int value) const;
		
	private:
		Parameter paramIndex_; // In the DW8000, this is really a list of 51 consecutive parameter bytes, no lookup necessary
		std::string parameterName_;
		int bits_;
		uint8 maxValue_;
		TValueLookup valueLookup_;
	};

}
