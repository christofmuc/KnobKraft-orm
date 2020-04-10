#include "MKS80_BCR2000.h"

#include "BCR2000.h"

#include <boost/format.hpp>

MKS80_BCR2000_Encoder::MKS80_BCR2000_Encoder(MKS80_Parameter::ParameterType paramType, MKS80_Parameter::SynthSection section, MKS80_Parameter::PatchParameter patchParameter, BCRtype type, int encoderNumber, BCRledMode ledMode /*= ONE_DOT*/) :
	MKS80_BCR2000_Encoder(paramType, section, (int)patchParameter, type, encoderNumber, ledMode)
{
}

MKS80_BCR2000_Encoder::MKS80_BCR2000_Encoder(MKS80_Parameter::ParameterType paramType, MKS80_Parameter::SynthSection section, MKS80_Parameter::ToneParameter toneParameter, BCRtype type, int encoderNumber, BCRledMode ledMode /*= ONE_DOT*/) :
	MKS80_BCR2000_Encoder(paramType, section, (int)toneParameter, type, encoderNumber, ledMode)
{
}


MKS80_BCR2000_Encoder::MKS80_BCR2000_Encoder(MKS80_Parameter::ParameterType paramType, MKS80_Parameter::SynthSection section, int parameterIndex, BCRtype type, int encoderNumber, BCRledMode ledMode) : 
	BCRStandardDefinitionWithName(type, encoderNumber), section_(section), ledMode_(ledMode)
{
	param_ = MKS80_Parameter::findParameter(paramType, section_, parameterIndex);
	jassert(param_);
	if (param_) {
		setName(param_->description());
	}
}

std::string MKS80_BCR2000_Encoder::generateBCR(int channel) const {
	int toneOrPatch = param_->parameterType() == MKS80_Parameter::PATCH ? 0x30 : 0x20; // This is called Level in the sysex documentation
	int upperOrLower = param_->section() == MKS80_Parameter::LOWER ? 0x10 : 0x01; // Hard code to upper for now
	switch (type_) {
	case ENCODER:
		return (boost::format(
			"$encoder %d ; %s\n"
			"  .tx $F0 $41 $36 $%02X $20 $%02X $%02X $%02X val $F7\n"
			"  .minmax %d %d\n"
			"  .default 0\n"
			"  .mode %s\n"
			"  .showvalue on\n"
			"  .resolution 64 64 127 127\n"
		) % encoderNumber() % param_->name() % (channel & 0x0f) % toneOrPatch % upperOrLower % param_->paramIndex() % param_->minValue() % param_->maxValue() % BCRdefinition::ledMode(ledMode_)).str();
	case BUTTON: {
		std::string buttonMode = param_->maxValue() > 1 ? "incval 1" : "toggle";
		return (boost::format(
			"$button %d ; %s\n"
			"  .tx $F0 $41 $36 $%02X $20 $%02X $%02X $%02X val $F7\n"
			"  .minmax %d %d\n"
			"  .default 0\n"
			"  .mode %s\n"
			"  .showvalue on\n"
		) % encoderNumber() % param_->name() % (channel & 0x0f) % toneOrPatch % upperOrLower % param_->paramIndex() % param_->minValue() % param_->maxValue() % buttonMode).str();
	}
	default:
		return (boost::format(
			"; Undefined: %s\n"
		) % param_->name()).str();
	}
}


std::shared_ptr<MKS80_Parameter> MKS80_BCR2000_Encoder::parameterDef() const
{
	return param_;
}

// Create definition of BCR2000 setup for the MKS80
std::vector<BCRStandardDefinition *> MKS80_BCR2000::BCR2000_setup(MKS80_Parameter::SynthSection section) {
	static std::map<MKS80_Parameter::SynthSection, std::vector<BCRStandardDefinition *>> kMKS80_BCR2000_Setup;
	// Lazy initialization so I don't get into static initialization dependencies
	if (kMKS80_BCR2000_Setup.find(section) == kMKS80_BCR2000_Setup.end()) {
		kMKS80_BCR2000_Setup[section] = {
			new MKS80_BCR2000_Encoder(MKS80_Parameter::TONE, section, MKS80_Parameter::VCO1_RANGE, ENCODER, 1),
			new MKS80_BCR2000_Encoder(MKS80_Parameter::TONE, section, MKS80_Parameter::VCO1_WAVEFORM, BUTTON, 1),
			new MKS80_BCR2000_Encoder(MKS80_Parameter::TONE, section, MKS80_Parameter::PULSE_WIDTH, ENCODER, 2),
			new MKS80_BCR2000_Encoder(MKS80_Parameter::TONE, section, MKS80_Parameter::PWM_POLARITY, BUTTON, 2),
			new MKS80_BCR2000_Encoder(MKS80_Parameter::TONE, section, MKS80_Parameter::PULSE_WIDTH_MOD, ENCODER, 3),
			new MKS80_BCR2000_Encoder(MKS80_Parameter::TONE, section, MKS80_Parameter::PWM_MODE_SELECT, BUTTON, 3),
			new MKS80_BCR2000_Encoder(MKS80_Parameter::TONE, section, MKS80_Parameter::MIXER, ENCODER, 4, PAN),
			new MKS80_BCR2000_Encoder(MKS80_Parameter::TONE, section, MKS80_Parameter::VCO2_RANGE, ENCODER, 5),
			new MKS80_BCR2000_Encoder(MKS80_Parameter::TONE, section, MKS80_Parameter::VCO2_WAVEFORM, BUTTON, 5),
			new MKS80_BCR2000_Encoder(MKS80_Parameter::TONE, section, MKS80_Parameter::VCO_FINE_TUNE, ENCODER, 6),
			new MKS80_BCR2000_Encoder(MKS80_Parameter::TONE, section, MKS80_Parameter::VCO_KEY_FOLLOW, ENCODER, 7),
			new MKS80_BCR2000_Encoder(MKS80_Parameter::TONE, section, MKS80_Parameter::VCO_SELECT, BUTTON, 7),
			new MKS80_BCR2000_Encoder(MKS80_Parameter::TONE, section, MKS80_Parameter::XMOD_MANUAL_DEPTH, BUTTON, 8),

			new MKS80_BCR2000_Encoder(MKS80_Parameter::TONE, section, MKS80_Parameter::LFO1_RATE, ENCODER, 9),
			new MKS80_BCR2000_Encoder(MKS80_Parameter::TONE, section, MKS80_Parameter::LFO1_WAVEFORM, BUTTON, 9),
			new MKS80_BCR2000_Encoder(MKS80_Parameter::TONE, section, MKS80_Parameter::LFO1_DELAY_TIME, ENCODER, 10),
			new MKS80_BCR2000_Encoder(MKS80_Parameter::TONE, section, MKS80_Parameter::VCO_MOD_LFO1_DEPTH, ENCODER, 11),
			new MKS80_BCR2000_Encoder(MKS80_Parameter::TONE, section, MKS80_Parameter::VCA_MOD_LFO1_DEPTH, ENCODER, 12),

			new MKS80_BCR2000_Encoder(MKS80_Parameter::PATCH, section, MKS80_Parameter::KEY_MODE_SELECT, ENCODER, 17),
			new MKS80_BCR2000_Encoder(MKS80_Parameter::PATCH, section, MKS80_Parameter::SPLIT_POINT, ENCODER, 18),
			new MKS80_BCR2000_Encoder(MKS80_Parameter::PATCH, section, MKS80_Parameter::OCTAVE_SHIFT, ENCODER, 19),
			new MKS80_BCR2000_Encoder(MKS80_Parameter::PATCH, section, MKS80_Parameter::ASSIGN_MODE_SELECT, ENCODER, 20),
			new MKS80_BCR2000_Encoder(MKS80_Parameter::PATCH, section, MKS80_Parameter::HOLD, ENCODER, 21),
			new MKS80_BCR2000_Encoder(MKS80_Parameter::PATCH, section, MKS80_Parameter::BENDER_SENSIVITY, ENCODER, 22),
			new MKS80_BCR2000_Encoder(MKS80_Parameter::PATCH, section, MKS80_Parameter::AFTERTOUCH_SENSIVITY, ENCODER, 23),
			new MKS80_BCR2000_Encoder(MKS80_Parameter::PATCH, section, MKS80_Parameter::LFO2_RATE, ENCODER, 24),

			new MKS80_BCR2000_Encoder(MKS80_Parameter::PATCH, section, MKS80_Parameter::TONE_NUMBER, ENCODER, 25),

			new MKS80_BCR2000_Encoder(MKS80_Parameter::TONE, section, MKS80_Parameter::VCO1_MOD, BUTTON, 33),
			new MKS80_BCR2000_Encoder(MKS80_Parameter::PATCH, section, MKS80_Parameter::VCO1_BEND, BUTTON, 34),
			new MKS80_BCR2000_Encoder(MKS80_Parameter::TONE, section, MKS80_Parameter::VCO_SYNC, BUTTON, 36),
			new MKS80_BCR2000_Encoder(MKS80_Parameter::TONE, section, MKS80_Parameter::VCO2_MOD, BUTTON, 37),
			new MKS80_BCR2000_Encoder(MKS80_Parameter::PATCH, section, MKS80_Parameter::VCO2_BEND, BUTTON, 38),
			new MKS80_BCR2000_Encoder(MKS80_Parameter::TONE, section, MKS80_Parameter::XMOD_POLARITY, BUTTON, 40),

			new MKS80_BCR2000_Encoder(MKS80_Parameter::TONE, section, MKS80_Parameter::ENV_RESET, BUTTON, 41),
			new MKS80_BCR2000_Encoder(MKS80_Parameter::TONE, section, MKS80_Parameter::ENV1_DYNAMICS, BUTTON, 42),
			new MKS80_BCR2000_Encoder(MKS80_Parameter::TONE, section, MKS80_Parameter::ENV2_DYNAMICS, BUTTON, 43),
			new MKS80_BCR2000_Encoder(MKS80_Parameter::TONE, section, MKS80_Parameter::VCF_ENV_SELECT, BUTTON, 44),
			new MKS80_BCR2000_Encoder(MKS80_Parameter::TONE, section, MKS80_Parameter::VCF_ENV_POLARITY, BUTTON, 45),
			new MKS80_BCR2000_Encoder(MKS80_Parameter::PATCH, section, MKS80_Parameter::AFTERTOUCH_MODE_SELECT, BUTTON, 48),

			new MKS80_BCR2000_Encoder(MKS80_Parameter::TONE, section, MKS80_Parameter::VCF_CUTOFF_FREQ, ENCODER, 33),
			new MKS80_BCR2000_Encoder(MKS80_Parameter::TONE, section, MKS80_Parameter::VCF_RESONANCE, ENCODER, 34),
			new MKS80_BCR2000_Encoder(MKS80_Parameter::TONE, section, MKS80_Parameter::HPF_CUTOFF_FREQ, ENCODER, 35),
			new MKS80_BCR2000_Encoder(MKS80_Parameter::TONE, section, MKS80_Parameter::VCF_MOD_ENV_DEPTH, ENCODER, 36),
			new MKS80_BCR2000_Encoder(MKS80_Parameter::TONE, section, MKS80_Parameter::VCF_MOD_LFO1_DEPTH, ENCODER, 37),
			new MKS80_BCR2000_Encoder(MKS80_Parameter::TONE, section, MKS80_Parameter::VCF_KEY_FOLLOW, ENCODER, 38),
			new MKS80_BCR2000_Encoder(MKS80_Parameter::PATCH, section, MKS80_Parameter::UNISON_DETUNE, ENCODER, 39),
			new MKS80_BCR2000_Encoder(MKS80_Parameter::PATCH, section, MKS80_Parameter::BALANCE, ENCODER, 40),

			new MKS80_BCR2000_Encoder(MKS80_Parameter::TONE, section, MKS80_Parameter::ENV1_ATTACK, ENCODER, 41),
			new MKS80_BCR2000_Encoder(MKS80_Parameter::TONE, section, MKS80_Parameter::ENV1_DECAY, ENCODER, 42),
			new MKS80_BCR2000_Encoder(MKS80_Parameter::TONE, section, MKS80_Parameter::ENV1_SUSTAIN, ENCODER, 43),
			new MKS80_BCR2000_Encoder(MKS80_Parameter::TONE, section, MKS80_Parameter::ENV1_RELEASE, ENCODER, 44),
			new MKS80_BCR2000_Encoder(MKS80_Parameter::TONE, section, MKS80_Parameter::ENV1_KEY_FOLLOW, ENCODER, 45),
			new MKS80_BCR2000_Encoder(MKS80_Parameter::TONE, section, MKS80_Parameter::VCO_MOD_ENV1_DEPTH, ENCODER, 46),
			new MKS80_BCR2000_Encoder(MKS80_Parameter::TONE, section, MKS80_Parameter::XMOD_ENV1_DEPTH, ENCODER, 47),
			new MKS80_BCR2000_Encoder(MKS80_Parameter::PATCH, section, MKS80_Parameter::GLIDE, ENCODER, 48),

			new MKS80_BCR2000_Encoder(MKS80_Parameter::TONE, section, MKS80_Parameter::ENV2_ATTACK, ENCODER, 49),
			new MKS80_BCR2000_Encoder(MKS80_Parameter::TONE, section, MKS80_Parameter::ENV2_DECAY, ENCODER, 50),
			new MKS80_BCR2000_Encoder(MKS80_Parameter::TONE, section, MKS80_Parameter::ENV2_SUSTAIN, ENCODER, 51),
			new MKS80_BCR2000_Encoder(MKS80_Parameter::TONE, section, MKS80_Parameter::ENV2_RELEASE, ENCODER, 52),
			new MKS80_BCR2000_Encoder(MKS80_Parameter::TONE, section, MKS80_Parameter::ENV2_KEY_FOLLOW, ENCODER, 53),
			new MKS80_BCR2000_Encoder(MKS80_Parameter::TONE, section, MKS80_Parameter::VCA_ENV2, ENCODER, 54),
			new MKS80_BCR2000_Encoder(MKS80_Parameter::TONE, section, MKS80_Parameter::DYNAMICS_TIME, ENCODER, 55),
			new MKS80_BCR2000_Encoder(MKS80_Parameter::TONE, section, MKS80_Parameter::DYNAMICS_LEVEL, ENCODER, 56),
		};
	}
	return kMKS80_BCR2000_Setup[section];
}


std::string MKS80_BCR2000::generateBCL(std::string const &presetName, int channel, int storePreset /* = -1 */, int recallPreset /* = -1 */) 
{
	std::string result;
	result = BCR2000::generateBCRHeader();
	for (int i = 0; i < 2; i++) {
		result += BCR2000::generatePresetHeader((boost::format(presetName) % i).str(), storePreset + i);
		std::vector<std::pair<BCRdefinition *, std::string>> all_entries;
		for (auto controller : MKS80_BCR2000::BCR2000_setup((MKS80_Parameter::SynthSection) i)) { 
			all_entries.emplace_back(std::make_pair(controller, controller->generateBCR(channel)));
		}
		result += BCR2000::generateAllEncoders(all_entries);
		result += BCR2000::generateBCRFooter(storePreset + i);
	}
	result += BCR2000::generateBCREnd(recallPreset); 
	return result;
}

