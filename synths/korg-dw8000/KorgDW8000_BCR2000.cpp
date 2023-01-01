/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "KorgDW8000_BCR2000.h"

#include "BCR2000.h"
#include "BCR2000_Presets.h"

#include <vector>
#include <boost/format.hpp>

namespace midikraft {

	class KorgDW8000InitPatchDefinition : public BCRStandardDefinition {
	public:
		KorgDW8000InitPatchDefinition(BCRtype type, int number) : BCRStandardDefinition(type, number) {}

		virtual std::string generateBCR(int channel) const {
			int channelCode = 0x30 | (channel & 0x0f);
			return (boost::format("$button %d ; Init Patch\n"
				".tx $F0 $42 $%02X $03 $40 $00 $00 $1F $00 $00 $00 $00 $00 $00 $1F $00 $00 $00 $00 $00 $3F $00 $00 $00 $00 $00 $00 $1F $00 $1F $00 $00 $00 $00 $1F $00 $1F $00 $00 $00 $00 $00 $00 $00 $00 $00 $00 $00 $00 $00 $00 $00 $00 $00 $00 $00 $f7\n"
				".minmax 0 0\n"
				".default 0\n"
				".mode down\n"
				".showvalue off\n") % number_ % channelCode).str();
		}
	};

	std::vector<BCRStandardDefinition *> dw8000GeneralSetup = {
		new KorgDW80002BCR2000Definition(ENCODER, 1, KorgDW8000Parameter::MG_FREQUENCY),
		new KorgDW80002BCR2000Definition(ENCODER, 2, KorgDW8000Parameter::MG_DELAY),
		new KorgDW80002BCR2000Definition(BUTTON, 2, KorgDW8000Parameter::MG_WAVE_FORM),
		new KorgDW80002BCR2000Definition(ENCODER, 3, KorgDW8000Parameter::AUTO_BEND_TIME),
		new KorgDW80002BCR2000Definition(BUTTON, 3, KorgDW8000Parameter::AUTO_BEND_SELECT),
		new KorgDW80002BCR2000Definition(ENCODER, 4, KorgDW8000Parameter::AUTO_BEND_INTENSITY),
		new KorgDW80002BCR2000Definition(BUTTON, 4, KorgDW8000Parameter::AUTO_BEND_MODE),
		new KorgDW80002BCR2000Definition(ENCODER, 5, KorgDW8000Parameter::DELAY_FREQUENCY),
		new KorgDW80002BCR2000Definition(BUTTON, 5, KorgDW8000Parameter::DELAY_INTENSITY),
		new KorgDW80002BCR2000Definition(ENCODER, 6, KorgDW8000Parameter::DELAY_FEEDBACK),
		new KorgDW80002BCR2000Definition(ENCODER, 7, KorgDW8000Parameter::DELAY_FACTOR), // TimeFine, not coarse
		new KorgDW80002BCR2000Definition(BUTTON, 7, KorgDW8000Parameter::DELAY_TIME),
		new KorgDW80002BCR2000Definition(ENCODER, 8, KorgDW8000Parameter::DELAY_EFFECT_LEVEL),

		new KorgDW80002BCR2000Definition(BUTTON, 33, KorgDW8000Parameter::OSC1_WAVE_FORM),
		new KorgDW80002BCR2000Definition(BUTTON, 34, KorgDW8000Parameter::OSC1_OCTAVE),
		new KorgDW80002BCR2000Definition(BUTTON, 35, KorgDW8000Parameter::BEND_OSC),
		new KorgDW80002BCR2000Definition(BUTTON, 36, KorgDW8000Parameter::BEND_VCF),
		new KorgDW80002BCR2000Definition(BUTTON, 37, KorgDW8000Parameter::KBD_TRACK),
		new KorgDW80002BCR2000Definition(BUTTON, 38, KorgDW8000Parameter::POLARITY),
		new KorgDW80002BCR2000Definition(BUTTON, 40, KorgDW8000Parameter::ASSIGN_MODE),

		new KorgDW80002BCR2000Definition(BUTTON, 41, KorgDW8000Parameter::OSC2_WAVE_FORM),
		new KorgDW80002BCR2000Definition(BUTTON, 42, KorgDW8000Parameter::OSC2_OCTAVE),
		new KorgDW80002BCR2000Definition(BUTTON, 43, KorgDW8000Parameter::INTERVAL),
		new KorgDW80002BCR2000Definition(BUTTON, 44, KorgDW8000Parameter::DETUNE),
		new KorgDW80002BCR2000Definition(BUTTON, 45, KorgDW8000Parameter::AFTER_TOUCH_OSC_MG),
		new KorgDW80002BCR2000Definition(BUTTON, 46, KorgDW8000Parameter::AFTER_TOUCH_VCF),
		new KorgDW80002BCR2000Definition(BUTTON, 47, KorgDW8000Parameter::AFTER_TOUCH_VCA),

		new KorgDW80002BCR2000Definition(ENCODER, 33, KorgDW8000Parameter::CUTOFF),
		new KorgDW80002BCR2000Definition(ENCODER, 34, KorgDW8000Parameter::RESONANCE),
		new KorgDW80002BCR2000Definition(ENCODER, 35, KorgDW8000Parameter::MG_VCF),
		new KorgDW80002BCR2000Definition(ENCODER, 36, KorgDW8000Parameter::MG_OSC),
		new KorgDW80002BCR2000Definition(ENCODER, 37, KorgDW8000Parameter::OSC1_LEVEL),
		new KorgDW80002BCR2000Definition(ENCODER, 38, KorgDW8000Parameter::OSC2_LEVEL),
		new KorgDW80002BCR2000Definition(ENCODER, 39, KorgDW8000Parameter::NOISE_LEVEL),

		new KorgDW80002BCR2000Definition(ENCODER, 41, KorgDW8000Parameter::VCF_ATTACK),
		new KorgDW80002BCR2000Definition(ENCODER, 42, KorgDW8000Parameter::VCF_DECAY),
		new KorgDW80002BCR2000Definition(ENCODER, 43, KorgDW8000Parameter::VCF_BREAK_POINT),
		new KorgDW80002BCR2000Definition(ENCODER, 44, KorgDW8000Parameter::VCF_SLOPE),
		new KorgDW80002BCR2000Definition(ENCODER, 45, KorgDW8000Parameter::VCF_SUSTAIN),
		new KorgDW80002BCR2000Definition(ENCODER, 46, KorgDW8000Parameter::VCF_RELEASE),
		new KorgDW80002BCR2000Definition(ENCODER, 47, KorgDW8000Parameter::VCF_VELOCITY_SENSIVITY),
		new KorgDW80002BCR2000Definition(ENCODER, 48, KorgDW8000Parameter::EG_INTENSITY),

		new KorgDW80002BCR2000Definition(ENCODER, 49, KorgDW8000Parameter::VCA_ATTACK),
		new KorgDW80002BCR2000Definition(ENCODER, 50, KorgDW8000Parameter::VCA_DECAY),
		new KorgDW80002BCR2000Definition(ENCODER, 51, KorgDW8000Parameter::VCA_BREAK_POINT),
		new KorgDW80002BCR2000Definition(ENCODER, 52, KorgDW8000Parameter::VCA_SLOPE),
		new KorgDW80002BCR2000Definition(ENCODER, 53, KorgDW8000Parameter::VCA_SUSTAIN),
		new KorgDW80002BCR2000Definition(ENCODER, 54, KorgDW8000Parameter::VCA_RELEASE),
		new KorgDW80002BCR2000Definition(ENCODER, 55, KorgDW8000Parameter::VCA_VELOCITY_SENSIVITY),
		new KorgDW80002BCR2000Definition(ENCODER, 56, KorgDW8000Parameter::PORTAMENTO),

		new KorgDW8000InitPatchDefinition(BUTTON, 49)
	};

	std::string KorgDW80002BCR2000Definition::generateBCR(int channel) const
	{
		// Get the full definition
		auto paramDef = KorgDW8000Parameter::findParameter(param_);
		jassert(paramDef != nullptr);

		int code = 0x30 | channel; // Channel format message
		switch (type_) {
		case ENCODER:
			return (boost::format(
				"$encoder %d ; %s\n"
				"  .tx $F0 $42 $%02X $03 $41 $%02X val $F7\n"
				"  .minmax 0 %d\n"
				"  .default %s\n"
				"  .mode %s\n"
				"  .showvalue on\n"
				"  .resolution 64 64 127 127\n"
			) % number_ % paramDef->name() % code % param_ % paramDef->maxValue() % 0 % BCRdefinition::ledMode(ledMode_)).str();
		case BUTTON: {
			std::string buttonMode = paramDef->maxValue() > 1 ? "incval 1" : "toggle";
			return (boost::format(
				"$button %d ; %s\n"
				"  .tx $F0 $42 $%02X $03 $41 $%02X val $F7\n"
				"  .minmax 0 %d\n"
				"  .default 0\n"
				"  .mode %s\n"
				"  .showvalue on\n"

			) % number_ % paramDef->name() % code % param_ % paramDef->maxValue() % buttonMode).str();
		}
		default:
			return (boost::format(
				"; %s\n"
			) % paramDef->name()).str();
		}
	}

	std::string KorgDW8000BCR2000::generateBCL(int channel)
	{
		std::string result;
		result = BCR2000::generateBCRHeader();
		result += BCR2000::generatePresetHeader("KORG DW8000", BCR2000_Preset_Positions::DW8000);
		// Loop over all DW8000 parameters, and write out a proper encoder definition
		std::vector<std::pair<BCRdefinition *, std::string>> all_entries;
		for (auto controller : dw8000GeneralSetup) {
			all_entries.push_back(std::make_pair(controller, controller->generateBCR(channel)));
		}
		result += BCR2000::generateAllEncoders(all_entries);
		result += BCR2000::generateBCRFooter(BCR2000_Preset_Positions::DW8000);
		result += BCR2000::generateBCREnd(BCR2000_Preset_Positions::DW8000);
		return result;
	}

}
