/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "KawaiK3_BCR2000.h"

#include "Capability.h"
#include "KawaiK3.h"
#include "BCR2000.h"

#include "KawaiK3WaveParameter.h"

#include <fmt/format.h>
#include "SpdLogJuce.h"

namespace midikraft {

	KawaiK3BCRCCDefinition::KawaiK3BCRCCDefinition(BCRtype type, int encoderNumber, KawaiK3Parameter::Parameter param, int controllerNumber, int minValue, int maxValue) :
		KawaiK3BCR2000Definition(type, encoderNumber, param),
		impl(type, encoderNumber, controllerNumber, minValue, maxValue)
	{
	}

	std::string KawaiK3BCRCCDefinition::generateBCR(int channel) const
	{
		return impl.generateBCR(channel);
	}

	class K3InitPatchDefinition : public BCRStandardDefinition, public BCRNamedParameterCapability {
	public:
		K3InitPatchDefinition(BCRtype type, int number) : BCRStandardDefinition(type, number) {}

		virtual std::string generateBCR(int channel) const override {
			KawaiK3 k3;
			k3.setCurrentChannelZeroBased(juce::MidiDeviceInfo(), juce::MidiDeviceInfo(), channel);
			auto patch = KawaiK3Patch::createInitPatch();
			auto syx = k3.k3PatchToSysex(patch->data(), KawaiK3::kFakeEditBuffer.toZeroBasedDiscardingBank(), false);

			return fmt::format("$button {} ; Init Patch\n"
				"  .tx $F0 {} $f7 $c{:x} $00\n" // Notice the program change message, else the K3 will keep playing the edit buffer and not reload the program
				"  .minmax 0 0\n"
				"  .default 0\n"
				"  .mode down\n"
				"  .showvalue off\n", number_, BCR2000::syxToBCRString(syx), channel);
		}

		std::string name() override
		{
			return "Init Patch";
		}

	};

	//
	// More ideas for the layout
	//
	// Button 51 + 52 for Undo Redo? Maybe a button to press to store undo point?
	// Press Encoder 4, 6, 7, 8 to reset to middle value
	// Add missing MONO (UNISON) switch. That's only possible to patch via syx I fear.
	std::vector<BCRStandardDefinition*> k3Setup = {
		new KawaiK3BCR2000Definition(ENCODER, 1, KawaiK3Parameter::OSC1_WAVE_SELECT, ONE_DOT_OFF),
		new KawaiK3BCR2000Definition(ENCODER, 2, KawaiK3Parameter::OSC1_RANGE),
		new KawaiK3BCR2000Definition(ENCODER, 3, KawaiK3Parameter::PORTAMENTO_SPEED, BAR),
		new KawaiK3BCR2000Definition(ENCODER, 4, KawaiK3Parameter::OSC_AUTO_BEND, PAN),
		new KawaiK3BCR2000Definition(ENCODER, 5, KawaiK3Parameter::OSC2_WAVE_SELECT, ONE_DOT_OFF),
		new KawaiK3BCR2000Definition(ENCODER, 6, KawaiK3Parameter::OSC2_COARSE, PAN),
		new KawaiK3BCR2000Definition(ENCODER, 7, KawaiK3Parameter::OSC2_FINE, PAN),
		new KawaiK3BCR2000Definition(ENCODER, 8, KawaiK3Parameter::OSC_BALANCE, PAN),

		new KawaiK3BCR2000Definition(ENCODER, 9, KawaiK3Parameter::KCV_VCA, PAN),
		new KawaiK3BCR2000Definition(ENCODER, 10, KawaiK3Parameter::VELOCITY_VCA, BAR),
		new KawaiK3BCR2000Definition(ENCODER, 11, KawaiK3Parameter::PRESSURE_VCA, BAR),
		new KawaiK3BCR2000Definition(ENCODER, 12, KawaiK3Parameter::PITCH_BEND, BAR),
		new KawaiK3BCR2000Definition(ENCODER, 13, KawaiK3Parameter::PRESSURE_OSC_BALANCE, BAR),

		new KawaiK3BCR2000Definition(ENCODER, 19, KawaiK3Parameter::PITCH_BEND, BAR), // Duplicate for compatibility with Royce's layout

		new KawaiK3BCRWaveDefinition(25, DrawbarOrgan::hammondDrawbars()[0].harmonic_number_),
		new KawaiK3BCRWaveDefinition(26, DrawbarOrgan::hammondDrawbars()[1].harmonic_number_),
		new KawaiK3BCRWaveDefinition(27, DrawbarOrgan::hammondDrawbars()[2].harmonic_number_),
		new KawaiK3BCRWaveDefinition(28, DrawbarOrgan::hammondDrawbars()[3].harmonic_number_),
		new KawaiK3BCRWaveDefinition(29, DrawbarOrgan::hammondDrawbars()[4].harmonic_number_),
		new KawaiK3BCRWaveDefinition(30, DrawbarOrgan::hammondDrawbars()[5].harmonic_number_),
		new KawaiK3BCRWaveDefinition(31, DrawbarOrgan::hammondDrawbars()[6].harmonic_number_),
		new KawaiK3BCRWaveDefinition(32, DrawbarOrgan::hammondDrawbars()[7].harmonic_number_),

		new KawaiK3BCRCCDefinition(BUTTON,  3, KawaiK3Parameter::PORTAMENTO_SWITCH, 65, 0, 1), // Controller 65 is the portamento switch. Use it on the Portamento speed controller but also on the button below that
		new KawaiK3BCRCCDefinition(BUTTON, 35, KawaiK3Parameter::PORTAMENTO_SWITCH, 65, 0, 1), // Controller 65 is the portamento switch

		new KawaiK3BCR2000Definition(ENCODER, 33, KawaiK3Parameter::CUTOFF),
		new KawaiK3BCR2000Definition(ENCODER, 34, KawaiK3Parameter::RESONANCE),
		new KawaiK3BCR2000Definition(ENCODER, 35, KawaiK3Parameter::LOW_CUT, ONE_DOT_OFF),
		new KawaiK3BCR2000Definition(ENCODER, 36, KawaiK3Parameter::VCF_ENV),
		new KawaiK3BCR2000Definition(ENCODER, 37, KawaiK3Parameter::KCV_VCF),
		new KawaiK3BCR2000Definition(ENCODER, 38, KawaiK3Parameter::VELOCITY_VCF),
		new KawaiK3BCR2000Definition(ENCODER, 39, KawaiK3Parameter::PRESSURE_VCF),
		new KawaiK3BCR2000Definition(ENCODER, 40, KawaiK3Parameter::VCA_LEVEL),

		new KawaiK3BCR2000Definition(ENCODER, 41, KawaiK3Parameter::VCF_ATTACK),
		new KawaiK3BCR2000Definition(ENCODER, 42, KawaiK3Parameter::VCF_DECAY),
		new KawaiK3BCR2000Definition(ENCODER, 43, KawaiK3Parameter::VCF_SUSTAIN, ONE_DOT_OFF),
		new KawaiK3BCR2000Definition(ENCODER, 44, KawaiK3Parameter::VCF_RELEASE, ONE_DOT_OFF),
		new KawaiK3BCR2000Definition(ENCODER, 45, KawaiK3Parameter::VCA_ATTACK),
		new KawaiK3BCR2000Definition(ENCODER, 46, KawaiK3Parameter::VCA_DECAY),
		new KawaiK3BCR2000Definition(ENCODER, 47, KawaiK3Parameter::VCA_SUSTAIN, ONE_DOT_OFF),
		new KawaiK3BCR2000Definition(ENCODER, 48, KawaiK3Parameter::VCA_RELEASE, ONE_DOT_OFF),

		new KawaiK3BCR2000Definition(ENCODER, 49, KawaiK3Parameter::LFO_SHAPE),
		new KawaiK3BCR2000Definition(ENCODER, 50, KawaiK3Parameter::LFO_SPEED),
		new KawaiK3BCR2000Definition(ENCODER, 51, KawaiK3Parameter::LFO_DELAY, ONE_DOT_OFF),
		new KawaiK3BCR2000Definition(ENCODER, 52, KawaiK3Parameter::LFO_OSC, ONE_DOT_OFF),
		new KawaiK3BCR2000Definition(ENCODER, 53, KawaiK3Parameter::LFO_VCF, ONE_DOT_OFF),
		new KawaiK3BCR2000Definition(ENCODER, 54, KawaiK3Parameter::LFO_VCA, ONE_DOT_OFF),
		new KawaiK3BCR2000Definition(ENCODER, 55, KawaiK3Parameter::PRESSURE_LFO_OSC, ONE_DOT_OFF),
		new KawaiK3BCR2000Definition(ENCODER, 56, KawaiK3Parameter::CHORUS, ONE_DOT_OFF),

		new K3InitPatchDefinition(BUTTON, 49)
	};

	std::string KawaiK3BCR2000Definition::generateBCR(int knobkraftChannel) const
	{
		// Get the full definition
		auto paramDef = KawaiK3Parameter::findParameter(param_);
		jassert(paramDef != nullptr);

		int range = paramDef->maxValue();
		switch (type_) {
		case ENCODER: {
			if (paramDef->minValue() >= 0) {
				// Simple case, no negative values required
				return fmt::format(
					"$encoder {} ; {}\n"
					"  .easypar CC {} {} {} {} absolute\n"
					// This would work, but as negative numbers don't work we resort back to CC only and have knobkraft translate it
					//"  .tx $F0 $40 $%02X $10 $00 $01 $%02X val4.7 val0.3 $F7\n" 
					//"  .minmax 0 %d\n"
					"  .default {}\n"
					"  .mode {}\n"
					"  .showvalue on\n"
					"  .resolution {} {} {} {}\n"
				 , number_ , paramDef->name()
					, (knobkraftChannel + 1) , (int)paramDef->paramNo() , 0 , paramDef->maxValue()
					//% (channel & 0x0f) % param_ 
					//% paramDef->maxValue() 
					, 0 , BCRdefinition::ledMode(ledMode_)
					, range , range , range , range
					);
			}
			else {
				// Uh, the BCR can't do negative. So we need to shift the range from e.g. -15..15 to 0..31, but generate 0x8F..0x81 0 0x01 .. 0x0F
				// Is this possible? No, I don't think so. So we put just CC controller messages in place, which we will translate later
				CCBCRdefinition cc(number_, (int)paramDef->paramNo(), 0, paramDef->maxValue() - paramDef->minValue(), ledMode_);
				return cc.generateBCR(knobkraftChannel);
			}
		}
		case BUTTON:
			// Not implemented yet
		default:
			return fmt::format(
				"; {}\n",
			paramDef->name());
		}
	}

	std::shared_ptr<midikraft::SynthParameterDefinition> KawaiK3BCR2000Definition::parameter()
	{
		return KawaiK3Parameter::findParameter(param_);
	}

	std::string KawaiK3BCR2000::generateBCL(std::string const& presetName, MidiChannel knobkraftChannel, MidiChannel  k3Channel)
	{
		std::string result;
		result = BCR2000::generateBCRHeader();
		result += BCR2000::generatePresetHeader(presetName);
		// Loop over all parameters, and write out a proper encoder definition
		std::vector<std::pair<BCRdefinition*, std::string>> all_entries;
		for (auto controller : k3Setup) {
			int channelToUse = knobkraftChannel.toZeroBasedInt();
			// Exception: For the Syx code to set the init patch, use the real k3 channel because we do not need to translate that, as well as for the CC Portamento switch
			if (dynamic_cast<KawaiK3BCR2000Definition*>(controller) == nullptr) {
				channelToUse = k3Channel.toZeroBasedInt();
			}
			if (controller) {
				all_entries.push_back(std::make_pair(controller, controller->generateBCR(channelToUse)));
			}
			else {
				jassertfalse;
			}
		}
		result += BCR2000::generateAllEncoders(all_entries);
		result += BCR2000::generateBCRFooter(-1); //TODO - or should it go into a defined position? BCR2000_Preset_Positions::KAWAIK3);
		result += BCR2000::generateBCREnd(-1); // No need to recall
		return result;
	}

	juce::MidiMessage KawaiK3BCR2000::createMessageforParam(KawaiK3Parameter* paramDef, KawaiK3Patch const& patch, MidiChannel k3Channel)
	{
		// As the K3 has only 39 parameters, we use CC 1..39 to map these. Simple enough
		int value;
		if (paramDef->valueInPatch(patch, value)) {
			// For params with negative values, we offset!
			if (paramDef->minValue() < 0) {
				value = value - paramDef->minValue();
			}
			int controller = paramDef->paramNo();
			return MidiMessage::controllerEvent(k3Channel.toOneBasedInt(), controller, value);
		}
		else {
			jassertfalse;
			return MidiMessage();
		}
	}

    /*static int encoderNumber(KawaiK3BCR2000Definition* def);
	int encoderNumber(KawaiK3BCR2000Definition* def) {
		int encoder = -1;
		if (def->type() == ENCODER) {
			return def->encoderNumber();
		}
		return encoder;
	}*/

    static int buttonNumber(BCRStandardDefinition* def);
	int buttonNumber(BCRStandardDefinition* def) {
		int encoder = -1;
		if (def->type() == BUTTON) {
			return def->encoderNumber();
		}
		return encoder;
	}

	void KawaiK3BCR2000::setupBCR2000View(BCR2000Proxy* view, TypedNamedValueSet &parameterModel, ValueTree& valueTree)
	{
		ignoreUnused(valueTree);
		TypedNamedValueSet result;
		// Iterate over our definition and set the labels on the view to show the layout
		for (auto def : k3Setup) {
			auto bcrdef = midikraft::Capability::hasCapability<BCRGetParameterCapability>(def);
			if (bcrdef) {
				auto param = bcrdef->parameter();
				if (param) {
					if (parameterModel.hasValue(param->name())) {
						view->setRotaryParam(def->encoderNumber(), parameterModel.typedNamedValueByName(param->name()).get());
					}
				}
				else {
					jassertfalse;
				}
			}
			else {
				auto simpleDef = midikraft::Capability::hasCapability<BCRNamedParameterCapability>(def);
				if (simpleDef) {
					int button = buttonNumber(def);
					if (button != -1) {
						view->setButtonParam(button, simpleDef->name());
					}
				}
			}
		}
	}

	KawaiK3BCRWaveDefinition::KawaiK3BCRWaveDefinition(int encoderNumber, int harmonicNumber) : CCBCRdefinition(BCRtype::ENCODER, encoderNumber, 40 + harmonicNumber, 0, 31), harmonic_(harmonicNumber)
	{
	}

	std::string KawaiK3BCRWaveDefinition::name()
	{
		return fmt::format("Harmonic #{}", harmonic_);
	}

	std::shared_ptr<midikraft::SynthParameterDefinition> KawaiK3BCRWaveDefinition::parameter()
	{
		return std::make_shared<KawaiK3DrawbarParameters>(harmonic_);
	}

}
