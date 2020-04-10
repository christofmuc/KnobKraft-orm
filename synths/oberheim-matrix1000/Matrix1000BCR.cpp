#include "Matrix1000BCR.h"

#include "Matrix1000ParamDefinition.h"

#include "CCBCRDefinition.h"
#include "NRPNBCRdefinition.h"
#include "MidiHelpers.h"

#include <boost/format.hpp>

std::map<Matrix1000Param, BCRdefinition *> controllerSetup = {
	{ DCO_1_Initial_Frequency_LSB, new Matrix1000Encoder(1) } ,
	{ DCO_1_Click, new Matrix1000Button(1)  } ,
	{ DCO_1_Initial_Waveshape_0, new Matrix1000Encoder(2) },
	{ DCO_1_Waveform_Enable_Pulse, new Matrix1000Button(2, 16) }, // This includes DCO_1_Waveform_Enable_Saw, shared via MIDI CC 16 
	{ DCO_1_Initial_Pulse_width, new Matrix1000Encoder(3) },
	{ DCO_1_Waveform_Enable_Saw, new Matrix1000Button(3, 16) },  // This includes DCO_1_Waveform_Enable_Pulse, shared via MIDI CC 16
	{ MIX, new Matrix1000Encoder(4, PAN, true) }, // True sets the flip mode switching min and max (encoder works the opposite way)
	{ DCO_Sync_Mode, new Matrix1000Button(4) },

	{ DCO_2_Initial_Frequency_LSB, new Matrix1000Encoder(5) },
	{ DCO_2_Click, new Matrix1000Button(5) },
	{ DCO_2_Initial_Waveshape_0, new Matrix1000Encoder(6) },
	{ DCO_2_Waveform_Enable_Pulse, new Matrix1000Button(6, 12) }, // This includes DCO_1_Waveform_Enable_Saw, shared via MIDI CC 12
	{ DCO_2_Initial_Pulse_width, new Matrix1000Encoder(7) },
	{ DCO_2_Waveform_Enable_Saw, new Matrix1000Button(7, 12) }, // This includes DCO_1_Waveform_Enable_Pulse, shared via MIDI CC 12
	{ DCO_2_Detune, new Matrix1000Encoder(8) },

	{ LFO_1_Initial_Speed, new Matrix1000Encoder(9) },
	{ LFO_1_Waveshape, new Matrix1000Button(9) },
	{ LFO_1_Initial_Amplitude, new Matrix1000Encoder(10) },
	{ LFO_1_Trigger, new Matrix1000Button(10) },
	{ DCO_1_Freq_by_LFO_1_Amount, new Matrix1000Encoder(11) },
	{ DCO_2_Freq_by_LFO_1_Amount, new Matrix1000Encoder(12) },

	{ LFO_2_Initial_Speed, new Matrix1000Encoder(13) },
	{ LFO_2_Waveshape, new Matrix1000Button(13) },
	{ LFO_2_Initial_Amplitude, new Matrix1000Encoder(14) },
	{ LFO_2_Trigger, new Matrix1000Button(14) },
	{ DCO_1_PW_by_LFO_2_Amount, new Matrix1000Encoder(15) },
	{ DCO_2_PW_by_LFO_2_Amount, new Matrix1000Encoder(16) },

	{ Ramp_1_Rate, new Matrix1000Encoder(17) },
	{ Ramp1_Mode, new Matrix1000Button(17) },
	{ LFO_1_Amp_by_Ramp_1_Amount, new Matrix1000Encoder(18) },
	{ Ramp2_Rate, new Matrix1000Encoder(19) },
	{ Ramp2_Mode, new Matrix1000Button(19) },
	{ LFO_2_Amp_by_Ramp_2_Amount, new Matrix1000Encoder(20) },

	{ VCF_Freq_by_Pressure_Amount, new Matrix1000Encoder(21) },
	{ VCF_FM_Amount_by_Pressure_Amount, new Matrix1000Encoder(22) },
	{ LFO_1_Speed_by_Pressure_Amount, new Matrix1000Encoder(23) },
	// ENCODER 24 not used

	{ Portamento_Initial_Rate, new Matrix1000Encoder(25) },
	{ Exponential_2, new Matrix1000Button(25) },
	{ Portamento_rate_by_Velocity_Amount, new Matrix1000Encoder(26) },
	{ Legato_Portamento_Enable, new Matrix1000Button(26) },
	{ Env_1_Amplitude_by_Velocity_Amount, new Matrix1000Encoder(27) },
	{ Env_2_Amplitude_by_Velocity_Amount, new Matrix1000Encoder(28) },
	{ Env_3_Amplitude_by_Velocity_Amount, new Matrix1000Encoder(29) },
	{ VCA_1_by_Velocity_Amount, new Matrix1000Encoder(30) },
	{ LFO_2_Speed_by_Keyboard_Amount, new Matrix1000Encoder(31) },
	// ENCODER 32 not used

	{ VCF_Initial_Frequency_LSB, new Matrix1000Encoder(33) },
	{ VCF_Initial_Resonance, new Matrix1000Encoder(34) },
	{ VCF_FM_Initial_Amount, new  Matrix1000Encoder(35) },
	// ENCODER 36 unused
	// ENCODER 37 unused
	{ VCA_1_exponential_Initial_Amount, new  Matrix1000Encoder(38) },
	{ GliGliDetune, new  CCBCRdefinition(39, 0x5e, 0, 127) }, // This only works for Matrix with at least Firmware 1.16 (GliGli Unison Detune param)
	{ Volume, new  CCBCRdefinition(40, 0x07, 0, 127) }, // Normal Volume Control Change

	{ Keyboard_mode, new Matrix1000Button(33) },
	{ DCO_1_Fixed_Modulations_PitchBend, new Matrix1000Button(34) }, // This includes DCO_1_Fixed_Modulations_Vibrato
	{ DCO_1_Fixed_Modulations_Portamento, new Matrix1000Button(35) },
	{ DCO_2_Fixed_Modulations_PitchBend, new Matrix1000Button(36) }, // This includes DCO_2_Fixed_Modulations_Vibrato
	{ DCO_2_Fixed_Modulations_Portamento, new Matrix1000Button(37) },
	{ VCF_Fixed_Modulations_Lever1, new Matrix1000Button(38) }, // This includes VCF_Fixed_Modulations_Vibrato
	{ LFO_1_Lag_Enable, new Matrix1000Button(39) },
	{ LFO_2_Lag_Enable, new Matrix1000Button(40) },

	{ Env_1_Trigger_Mode_Bit0, new Matrix1000Button(41) }, // This includes Env_1_Trigger_Mode_Bit1 and Env_1_Trigger_Mode_Bit2
	{ Env_1_LFO_Trigger_Mode_Bit0, new Matrix1000Button(42) }, // This includes Env_1_LFO_Trigger_Mode_Bit1
	{ Env_1_Mode_Bit0, new Matrix1000Button(43) }, // This includes Env_1_Mode_Bit1
	// Button 44 not used
	{ Env_2_Trigger_Mode_Bit0, new Matrix1000Button(45) }, // This includes Env_2_Trigger_Mode_Bit1 and Env_2_Trigger_Mode_Bit2
	{ Env_2_LFO_Trigger_Mode_Bit0, new Matrix1000Button(46) }, // This includes Env_2_LFO_Trigger_Mode_Bit1
	{ Env_2_Mode_Bit0, new Matrix1000Button(47) }, // This includes Env_2_Mode_Bit1
	// Button 48 not used

	{ Env_1_Initial_Delay_Time, new Matrix1000Encoder(41) },
	{ Env_1_Initial_Attack_Time, new Matrix1000Encoder(42) },
	{ Env_1_Initial_Decay_Time, new Matrix1000Encoder(43) },
	{ Env_1_Sustain_Level, new Matrix1000Encoder(44) },
	{ Env_1_Initial_Release_Time, new Matrix1000Encoder(45) },
	{ Env_1_Initial_Amplitude, new Matrix1000Encoder(46) },
	{ VCF_Freq_by_Env_1_Amount, new Matrix1000Encoder(47) },
	// Encoder 48 unused

	{ Env_2_Initial_Delay_Time, new Matrix1000Encoder(49) },
	{ Env_2_Initial_Attack_Time, new Matrix1000Encoder(50) },
	{ Env_2_Initial_Decay_Time, new Matrix1000Encoder(51) },
	{ Env_2_Sustain_Level, new Matrix1000Encoder(52) },
	{ Env_2_Initial_Release_Time, new Matrix1000Encoder(53) },
	{ Env_2_Initial_Amplitude, new Matrix1000Encoder(54) },
	{ VCA_2_by_Env_2_Amount, new Matrix1000Encoder(55) },
	// Encoder 56 unused
};

// This is the overflow page of all functions that do not fit into the first Behringer setup
std::map<Matrix1000Param, BCRdefinition *> secondControllerSetup = {
	// Editing the Mod Matrix is not possible, because we would need to put three values into a single sysex string
	// The BCR2000 cannot do that, unfortunately.
	/*{ M_Bus_0_Amount, new Matrix1000Encoder(1) },
	{ Matrix_Modulation_Bus_0_Source_Code, new Matrix1000Button(33) },
	{ MM_Bus_0_Destination_Code, new Matrix1000Button(41) },
	{ M_Bus_1_Amount, new Matrix1000Encoder(2) },
	{ Matrix_Modulation_Bus_1_Source_Code, new Matrix1000Button(34) },
	{ MM_Bus_1_Destination_Code, new Matrix1000Button(42) },
	{ M_Bus_2_Amount, new Matrix1000Encoder(3) },
	{ Matrix_Modulation_Bus_2_Source_Code, new Matrix1000Button(35) },
	{ MM_Bus_2_Destination_Code, new Matrix1000Button(43) },
	{ M_Bus_3_Amount, new Matrix1000Encoder(4) },
	{ Matrix_Modulation_Bus_3_Source_Code, new Matrix1000Button(36) },
	{ MM_Bus_3_Destination_Code, new Matrix1000Button(44) },
	{ M_Bus_4_Amount, new Matrix1000Encoder(5) },
	{ Matrix_Modulation_Bus_4_Source_Code, new Matrix1000Button(37) },
	{ MM_Bus_4_Destination_Code, new Matrix1000Button(45) },
	{ M_Bus_5_Amount, new Matrix1000Encoder(6) },
	{ Matrix_Modulation_Bus_5_Source_Code, new Matrix1000Button(38) },
	{ MM_Bus_5_Destination_Code, new Matrix1000Button(46) },
	{ M_Bus_6_Amount, new Matrix1000Encoder(7) },
	{ Matrix_Modulation_Bus_6_Source_Code, new Matrix1000Button(39) },
	{ MM_Bus_6_Destination_Code, new Matrix1000Button(47) },
	{ M_Bus_7_Amount, new Matrix1000Encoder(8) },
	{ Matrix_Modulation_Bus_7_Source_Code, new Matrix1000Button(40) },
	{ MM_Bus_7_Destination_Code, new Matrix1000Button(48) },

	{ M_Bus_8_Amount, new Matrix1000Encoder(9) },
	{ Matrix_Modulation_Bus_8_Source_Code, new Matrix1000Encoder(10) },
	{ MM_Bus_8_Destination_Code, new Matrix1000Encoder(11) },
	{ M_Bus_9_Amount, new Matrix1000Encoder(12) },
	{ Matrix_Modulation_Bus_9_Source_Code, new Matrix1000Encoder(13) },
	{ MM_Bus_9_Destination_Code, new Matrix1000Encoder(14) },
	// ENCODER 15 unused
	// ENCODER 16 unused*/

	{ LFO_1_Retrigger_point, new Matrix1000Encoder(1) },
	{ LFO_1_Sampled_Source_Number, new Matrix1000Encoder(2) },
	{ LFO_2_Retrigger_point, new Matrix1000Encoder(3) },
	{ LFO_2_Sampled_Source_Number, new Matrix1000Encoder(4) },
	// ENCODER 5 unused
	// ENCODER 6 unused
	// ENCODER 7 unused
	// ENCODER 8 unused

	{ Tracking_Generator_Input_Source_Code, new Matrix1000Encoder(33) },
	{ Tracking_Point_1, new Matrix1000Encoder(34) },
	{ Tracking_Point_2, new Matrix1000Encoder(35) },
	{ Tracking_Point_3, new Matrix1000Encoder(36) },
	{ Tracking_Point_4, new Matrix1000Encoder(37) },
	// ENCODER 38 unused
	// ENCODER 39 unused
	// ENCODER 40 unused

	{ Env_3_Initial_Delay_Time, new Matrix1000Encoder(49) },
	{ Env_3_Initial_Attack_Time, new Matrix1000Encoder(50) },
	{ Env_3_Initial_Decay_Time, new Matrix1000Encoder(51) },
	{ Env_3_Sustain_Level, new Matrix1000Encoder(52) },
	{ Env_3_Initial_Release_Time, new Matrix1000Encoder(53) },
	{ Env_3_Initial_Amplitude, new Matrix1000Encoder(54) },
	{ VCF_FM_Amount_by_Env_3_Amount, new Matrix1000Encoder(55) },
	// Encoder 56 unused
};


Matrix1000BCR::Matrix1000BCR(int midiChannel, bool useSysex /*= true*/) : channel_(midiChannel), useSysex_(useSysex)
{
}

std::string Matrix1000BCR::generateBCR(std::string const &preset1, std::string const &preset2, int baseStoragePlace, bool includeHeaderAndFooter)
{
	std::string result;
	if (includeHeaderAndFooter) result = BCR2000::generateBCRHeader();
	result += generateMapping(preset1, controllerSetup, baseStoragePlace + 0);
	result += generateMapping(preset2, secondControllerSetup, baseStoragePlace + 1);
	if (includeHeaderAndFooter) result += BCR2000::generateBCREnd(baseStoragePlace);
	return result;
}

bool Matrix1000BCR::parameterHasControlAssigned(Matrix1000Param const param) 
{
	if (controllerSetup.find(param) != controllerSetup.end()) return true;
	if (secondControllerSetup.find(param) != secondControllerSetup.end()) return true;
	return false;
}

std::vector<juce::MidiMessage> Matrix1000BCR::createNRPNforParam(Matrix1000ParamDefinition *param, Patch const &patch, int zeroBasedChannel) 
{
	// This only works for Firmware 1.20 and 1.16, which accept NRPN to set the values
	int nrpn_value = patch.at(param->sysexIndex());
	if (abs(param->bits()) < 7 || param->bits() < 0) {
		nrpn_value += 0x40;
	}
	return MidiHelpers::generateRPN(zeroBasedChannel + 1, param->controller(), nrpn_value, true, false, false);
}

std::string Matrix1000BCR::generateMapping(std::string const &presetName, std::map<Matrix1000Param, BCRdefinition *> &controllerSetup, int storagePlace) {
	std::string result = BCR2000::generatePresetHeader(presetName, storagePlace);

	// Loop over all Matrix 1000 Parameters, and write out a proper encoder definition
	std::vector<std::pair<BCRdefinition *, std::string>> all_entries;
	for (int index = 0; index < LAST; index++) {
		Matrix1000Param paramIndex = (Matrix1000Param)index;
		if (controllerSetup.find(paramIndex) != controllerSetup.end()) {
			// This is a parameter that is mapped to the BCR2000
			auto def = controllerSetup[paramIndex];
			auto m1000bcr = dynamic_cast<Matrix1000BCRDefinition *>(def);
			if (m1000bcr) {
				// Need to find the appropriate param definition
				for (auto param : Matrix1000ParamDefinition::allDefinitions) {
					auto m1000param = dynamic_cast<Matrix1000ParamDefinition *>(param);
					if (m1000param) {
						if (m1000param->id() == paramIndex) {
							if (useSysex_) {
								all_entries.push_back(std::make_pair(def, m1000bcr->generateBCR(m1000param)));
							}
							else {

								int minValue = m1000bcr->minValue(m1000param);
								int maxValue = m1000bcr->maxValue(m1000param);
								int defaultValue = m1000bcr->defaultValue(m1000param);
								if (m1000param->bits() > 0 && m1000param->bits() < 7) {
									// Positive values are offset by 0x40 to make them consistent with the full range of negative values...
									// This is special in Bob Grieb's firmware 1.20
									// Only works for the values with less than 7 bits
									minValue += 0x40;
									maxValue += 0x40;
									defaultValue += 0x40;
								}
								// We will not use sysex, which would be the native format for the Matrix1000, but rather rely 
								// on the NRPN implementation of the 1.20 firmware
								NRPNBCRdefinition nrpn(m1000param->description(), 
									m1000bcr->type(),
									m1000bcr->encoderNumber(),
									m1000param->controller(), 
									minValue, 
									maxValue,
									defaultValue,
									m1000bcr->ledMode());
								all_entries.push_back(std::make_pair(def, nrpn.generateBCR(channel_)));
							}
							break;
						}
					}
				}
			}
			else {
				// This is a standard BCR definition, and does not need the Matrix specific information
				// But it needs a MIDI channel...
				auto standard = dynamic_cast<BCRStandardDefinition *>(def);
				if (standard != nullptr) {
					all_entries.push_back(std::make_pair(standard, standard->generateBCR(channel_)));
				}
				else {
					jassert(false);
				}
			}
		}
	}

	result += BCR2000::generateAllEncoders(all_entries);

	return result + BCR2000::generateBCRFooter(storagePlace);
}

std::string Matrix1000BCRDefinition::ledMode() const {
	std::string ledMode = BCRdefinition::ledMode(ledMode_);
	if (number_ > 32 && ledMode_ > ONE_DOT_OFF) {
		// The elaborated functions of the LED ring are sadly not possible on the lower encoders
		ledMode = BCRdefinition::ledMode(ONE_DOT);
	}
	return ledMode;
}

int Matrix1000BCRDefinition::maxValue(Matrix1000ParamDefinition const *param) const {
	if (flip_)
		return minValueImpl(param);
	else
		return maxValueImpl(param);
}

int Matrix1000BCRDefinition::maxValueImpl(Matrix1000ParamDefinition const *param) const {
	switch (type_) {
	case ENCODER: {
		if (param->bits() > 0) {
			return (1 << param->bits()) - 1;
		}
		else {
			// Negative values go to 127
			return 127;
		}
	}
	case BUTTON: {
		if (param->bits() > 0) {
			return (1 << param->bits()) - 1;
		}
		else {
			// I don't think this should be a valid combination?
			jassert(false);
			return 1;
		}
	}
	}
	jassert(false);
	return -1;
}

int Matrix1000BCRDefinition::minValue(Matrix1000ParamDefinition const *param) const {
	if (flip_)
		return maxValueImpl(param);
	else
		return minValueImpl(param);
}

int Matrix1000BCRDefinition::defaultValue(Matrix1000ParamDefinition const *param) const
{
	if (param->bits() < 0)
		return maxValue(param) / 2 + 1;
	else
		return 0;
}

int Matrix1000BCRDefinition::minValueImpl(Matrix1000ParamDefinition const *param) const {
	// Is this actually true?
	return 0;
}

std::string Matrix1000BCRDefinition::generateBCR(Matrix1000ParamDefinition const *param) const
{
	// If the setting has a "virtual CC" controller, we are using the parameter sync feature of the BCR2000 by artificially
	// specifying a CC controller as easypar before the actual tx statement. 
	std::string virtualCC;
	int unusedChannel = 15; // Just so that nobody actually would interpret these CC messages?

	switch (type_) {
	case ENCODER: {
		if (virtualCC_ != -1) {
			virtualCC = (boost::format("  .easypar CC %d %d %d %d absolute\n") % virtualCC_ % unusedChannel % minValue(param) % maxValue(param)).str();
		}

		if (param->controller() < 0) {
			jassert(false);
		}

		if (param->bits() < 0) {
			//
			// This is unbelievable tricky, what a hack! Love it! 
			// This is the guy! https://groups.yahoo.com/neo/groups/bc2000/conversations/topics/5513
			std::vector<uint8> data = { 0xf7, 0xf0, 0x10, 0x06, 0x06 };

			// Ok, to carry the sum of the controller value plus $40, which is stored at address 4 in the sysex, 
			// forward from the illegal sysex message (being ignored) into the legal message, we need to 
			// use the checksum function and must make sure that the checksum is 0 of everything but the
			// value at position 4...
			uint8 sum = 0;
			for (auto &value : data) sum += value;
			sum += (uint8) param->controller();
			uint8 magic = (0x00 - sum) & 0x7f; // Because the sum of the data plus the magic must be 0. And the highest bit must not be set in a sysex message
			return (boost::format(
				"$encoder %d ; %s\n"
				"%s"
				"  .tx $F0 $7D $40 val cks-2 2 $%02X $F7 $F0 $10 $06 $06 $%02X cks-2 4 $F7\n"
				"  .minmax %d %d\n"
				"  .default %d\n"
				"  .mode %s\n"
				"  .showvalue on\n"
				"  .resolution 64 64 127 127\n"
			) % number_ % param->description() % virtualCC % (int) magic % param->controller() % minValue(param) % maxValue(param) % defaultValue(param) % ledMode()).str();
		}
		else {
			// The VCF frequency is the only parameter that is controlled with a proper 0..127 bits without sign extension,
			// so we can spare us the hack above for the negative numbers
			// Actually, we can use this for all unsigned values
			return (boost::format(
				"$encoder %d ; %s\n"
				"%s"
				"  .tx $F0 $10 $06 $06 $%02X val $F7\n"
				"  .minmax %d %d\n"
				"  .default %d\n"
				"  .mode %s\n"
				"  .showvalue on\n"
				"  .resolution 64 64 127 127\n"
			) % number_ % param->description() % virtualCC % param->controller() % minValue(param) % maxValue(param) % defaultValue(param) % ledMode()).str();
		}
	}
	case BUTTON: {
		auto buttonMode = "toggle";
		if (param->bits() > 1) {
			buttonMode = "incval 1";
		}
		if (virtualCC_ != -1) {
			virtualCC = (boost::format("  .easypar CC %d %d %d %d increment 1\n") % virtualCC_ % unusedChannel % maxValue(param) % minValue(param)).str(); // Note the flipped Min and Max values for the button
		}
		// I don't think buttons are allowed to flip minmax?
		jassert(!flip_);
		return (boost::format(
			"$button %d ; %s\n"
			"%s"
			"  .tx $F0 $10 $06 $06 $%02X val $F7\n"
			"  .minmax %d %d\n"
			"  .default 0\n"
			"  .mode %s\n"
			"  .showvalue on\n"
		) % number_ % param->description() % virtualCC % param->controller() % minValue(param) % maxValue(param) % buttonMode).str();
	}
	}
	jassert(false);
	return "";
}

Matrix1000Encoder::Matrix1000Encoder(int encoderNumber, BCRledMode ledMode /*= BAR*/, bool flip /*= false*/) : Matrix1000BCRDefinition(ENCODER, encoderNumber)
{
	setFlipMinMax(flip);  
	setLedMode(ledMode);
}

Matrix1000Button::Matrix1000Button(int buttonNumber, int virtualCC /*= -1*/) : Matrix1000BCRDefinition(BUTTON, buttonNumber, virtualCC)
{
}
