/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "BCRDefinition.h"
#include "CCBCRDefinition.h"
#include "BCR2000Proxy.h"
#include "KawaiK3Parameter.h"
#include "KawaiK3Patch.h"

#include "MidiChannel.h"

namespace midikraft {

	class KawaiK3BCR2000Definition : public BCRStandardDefinition {
	public:
		KawaiK3BCR2000Definition(BCRtype type, int number, KawaiK3Parameter::Parameter param, BCRledMode ledMode = ONE_DOT) :
			BCRStandardDefinition(type, number), param_(param), ledMode_(ledMode) {}

		virtual std::string generateBCR(int channel) const;

		KawaiK3Parameter::Parameter param() const { return param_; }

	protected:
		KawaiK3Parameter::Parameter param_;
		BCRledMode ledMode_;
	};

	class KawaiK3BCRCCDefinition : public KawaiK3BCR2000Definition {
	public:
		KawaiK3BCRCCDefinition(BCRtype type, int encoderNumber, KawaiK3Parameter::Parameter param, int controllerNumber, int minValue, int maxValue);

		virtual std::string generateBCR(int channel) const;

	private:
		CCBCRdefinition impl;
	};


	class KawaiK3BCR2000 {
	public:
		// As the BCR2000 is not really powerful enough to set a sign bit, it will only work properly with the KawaiK3 while the KnobKraft software is running
		static std::string generateBCL(std::string const &presetName, MidiChannel knobkraftChannel, MidiChannel k3Channel);
		static juce::MidiMessage createMessageforParam(KawaiK3Parameter *paramDef, KawaiK3Patch const &patch, MidiChannel k3Channel);
		static void setupBCR2000View(BCR2000Proxy *view);
		static void setupBCR2000Values(BCR2000Proxy *view, std::shared_ptr<DataFile> patch);
	};

}
