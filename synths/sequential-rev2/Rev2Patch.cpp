/*
   Copyright (c) 2019 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "Rev2Patch.h"

#include "Sysex.h"
#include "Rev2.h"

#include "BinaryResources.h"

#include <boost/format.hpp>
#include <boost/algorithm/string.hpp>

namespace midikraft {

	std::vector<NrpnDefinition> nrpns = {
		NrpnDefinition(0, 0, 120, "Osc 1 Freq", 0),
		NrpnDefinition(1, 0, 100, "Osc 1 Freq Fine", 2),
		NrpnDefinition(2, 0, 4, "Osc 1 Shape", 4, { {0, "Off"}, { 1, "Saw" }, { 2, "Saw+Triangle"}, { 3, "Triangle" }, { 4, "Pulse" } }),
		NrpnDefinition(3, 0, 127, "Osc 1 Glide", 8),
		NrpnDefinition(4, 0, 1, "Osc 1 KBD on/off", 10),
		NrpnDefinition(5, 0, 120, "Osc 2 Freq", 1),
		NrpnDefinition(6, 0, 100, "Osc 2 Freq Fine", 3),
		NrpnDefinition(7, 0, 4, "Osc 2 Shape", 5, { { 0, "Off" },{ 1, "Saw" },{ 2, "Saw+Triangle" },{ 3, "Triangle" },{ 4, "Pulse" } }),
		NrpnDefinition(8, 0, 127, "Osc 2 Glide", 9),
		NrpnDefinition(9, 0, 1, "Osc. 2 KBD on/off", 11),
		NrpnDefinition(10, 0, 1, "Sync On/Off", 17),
		NrpnDefinition(11, 0, 3, "Glide Mode", 18),
		NrpnDefinition(12, 0, 127, "Slop", 21),
		NrpnDefinition(13, 0, 127, "Osc Mix", 14),
		NrpnDefinition(14, 0, 127, "Noise", 16),
		NrpnDefinition(15, 0, 164, "Filter Cutoff", 22),
		NrpnDefinition(16, 0, 127, "Filter Resonance", 23),
		NrpnDefinition(17, 0, 127, "Keyboard Tracking", 24),
		NrpnDefinition(18, 0, 127, "Audio Mod", 25),
		NrpnDefinition(19, 0, 1, "2 pole/4 pole mode", 26),
		NrpnDefinition(20, 0, 254, "Filter Env Amt", 32),
		NrpnDefinition(21, 0, 127, "Filter Env Vel", 35),
		NrpnDefinition(22, 0, 127, "Filter Env Delay", 38),
		NrpnDefinition(23, 0, 127, "Filter Env Attack", 41),
		NrpnDefinition(24, 0, 127, "Filter Env Decay", 44),
		NrpnDefinition(25, 0, 127, "Filter Env Sustain", 47),
		NrpnDefinition(26, 0, 127, "Filter Env Release", 50),
		// 27 is really empty.If you try to set this, you get a change in index #33
		NrpnDefinition(28, 0, 127, "Pan Spread", 29),
		NrpnDefinition(29, 0, 127, "Voice Volume", 28),
		NrpnDefinition(30, 0, 127, "VCA Env Amt", 33),
		NrpnDefinition(31, 0, 127, "VCA Env Vel", 36),
		NrpnDefinition(32, 0, 127, "VCA Env Delay", 39),
		NrpnDefinition(33, 0, 127, "VCA Env Attack", 42),
		NrpnDefinition(34, 0, 127, "VCA Env Decay", 45),
		NrpnDefinition(35, 0, 127, "VCA Env Sustain", 48),
		NrpnDefinition(36, 0, 127, "VCA Env Release", 51),
		NrpnDefinition(37, 0, 127, "LFO 1 Freq", 53),
		NrpnDefinition(38, 0, 4, "LFO 1 Shape", 57),
		NrpnDefinition(39, 0, 127, "LFO 1 Amount", 61),
		NrpnDefinition(40, 0, 52, "LFO 1 Destination", 65),
		NrpnDefinition(41, 0, 1, "LFO 1 Clock Sync", 69),
		NrpnDefinition(42, 0, 150, "LFO 2 Freq", 54),
		NrpnDefinition(43, 0, 4, "LFO 2 Shape", 58),
		NrpnDefinition(44, 0, 127, "LFO 2 Amount", 62),
		NrpnDefinition(45, 0, 52, "LFO 2 Destination", 66),
		NrpnDefinition(46, 0, 1, "LFO 2 Clock Sync", 70),
		NrpnDefinition(47, 0, 150, "LFO 3 Freq", 55),
		NrpnDefinition(48, 0, 4, "LFO 3 Shape", 59),
		NrpnDefinition(49, 0, 127, "LFO 3 Amount", 63),
		NrpnDefinition(50, 0, 52, "LFO 3 Destination", 67),
		NrpnDefinition(51, 0, 1, "LFO 3 Clock Sync", 71),
		NrpnDefinition(52, 0, 150, "LFO 4 Freq", 56),
		NrpnDefinition(53, 0, 4, "LFO 4 Shape", 60),
		NrpnDefinition(54, 0, 127, "LFO 4 Amount", 64),
		NrpnDefinition(55, 0, 52, "LFO 4 Destination", 68),
		NrpnDefinition(56, 0, 1, "LFO 4 Clock Sync", 72),
		NrpnDefinition(57, 0, 52, "Env 3 Destination", 30),
		NrpnDefinition(58, 0, 254, "Env 3 Amount", 34),
		NrpnDefinition(59, 0, 127, "Env 3 Vel", 37),
		NrpnDefinition(60, 0, 127, "Env 3 Delay", 40),
		NrpnDefinition(61, 0, 127, "Env 3 Attack", 43),
		NrpnDefinition(62, 0, 127, "Env 3 Decay", 46),
		NrpnDefinition(63, 0, 127, "Env 3 Sustain", 49),
		NrpnDefinition(64, 0, 127, "Env 3 Release", 52),
		NrpnDefinition(65, 0, 22, "Mod 1 Source", 77),
		NrpnDefinition(66, 0, 254, "Mod 1 Amount", 85),
		NrpnDefinition(67, 0, 52, "Mod 1 Destination", 93),
		NrpnDefinition(68, 0, 22, "Mod 2 Source", 78),
		NrpnDefinition(69, 0, 254, "Mod 2 Amount", 86),
		NrpnDefinition(70, 0, 52, "Mod 2 Destination", 94),
		NrpnDefinition(71, 0, 22, "Mod 3 Source", 79),
		NrpnDefinition(72, 0, 254, "Mode 3 Amount", 87),
		NrpnDefinition(73, 0, 52, "Mode 3 Destination", 95),
		NrpnDefinition(74, 0, 22, "Mod 4 Source", 80),
		NrpnDefinition(75, 0, 254, "Mod 4 Amount", 88),
		NrpnDefinition(76, 0, 52, "Mod 4 Destination", 96),
		NrpnDefinition(77, 0, 22, "Mod 5 Source", 81),
		NrpnDefinition(78, 0, 254, "Mod 5 Amount", 89),
		NrpnDefinition(79, 0, 52, "Mod 5 Destination", 97),
		NrpnDefinition(80, 0, 22, "Mod 6 Source", 82),
		NrpnDefinition(81, 0, 254, "Mod 6 Amount", 90),
		NrpnDefinition(82, 0, 52, "Mod 6 Destination", 98),
		NrpnDefinition(83, 0, 22, "Mod 7 Source", 83),
		NrpnDefinition(84, 0, 254, "Mod 7 Amount", 91),
		NrpnDefinition(85, 0, 52, "Mod 7 Destination", 99),
		NrpnDefinition(86, 0, 22, "Mod 8 Source", 84),
		NrpnDefinition(87, 0, 254, "Mod 8 Amount", 92),
		NrpnDefinition(88, 0, 52, "Mod 8 Destination", 100),
		// TODO - really no values ?
		NrpnDefinition(97, 0, 1, "Env 3 Repeat On/Off", 31),
		// TODO - really no values ?
		NrpnDefinition(99, 0, 1, "Osc 1 Note Reset", 12),
		// TODO - really no values ?
		NrpnDefinition(102, 0, 99, "Osc 1 Pulse Width", 6),
		NrpnDefinition(103, 0, 99, "Osc 2 Pulse Width", 7),
		NrpnDefinition(104, 0, 1, "Osc 2 Note Reset", 13),
		NrpnDefinition(105, 0, 1, "LFO 1 Key Sync", 73),
		NrpnDefinition(106, 0, 1, "LFO 2 Key Sync", 74),
		NrpnDefinition(107, 0, 1, "LFO 3 Key Sync", 75),
		NrpnDefinition(108, 0, 1, "LFO 4 Key Sync", 76),
		// TODO - really no values ?
		NrpnDefinition(111, 0, 1, "Glide On/Off", 19),
		// TODO - really no values ?
		NrpnDefinition(113, 0, 12, "Pitch Bend Range", 20),
		NrpnDefinition(114, 0, 1, "Pan Mode", 209),
		// TODO - really no values ?
		NrpnDefinition(116, 0, 254, "Mod Wheel Amount", 101),
		NrpnDefinition(117, 0, 52, "Mod Wheel Dest", 102),
		NrpnDefinition(118, 0, 254, "Pressure Amount", 103),
		NrpnDefinition(119, 0, 52, "Pressure Dest", 104),
		NrpnDefinition(120, 0, 254, "Breath Amount", 105),
		NrpnDefinition(121, 0, 52, "Breath Dest", 106),
		NrpnDefinition(122, 0, 254, "Velocity Amount", 107),
		NrpnDefinition(123, 0, 52, "Velocity Dest", 108),
		NrpnDefinition(124, 0, 254, "Foot Ctrl Amount", 109),
		NrpnDefinition(125, 0, 52, "Foot Ctrl Dest", 110),
		// TODO - really no values ?
		NrpnDefinition(153, 0, 1, "FX On/Off", 116),
		NrpnDefinition(154, 0, 13, "FX Select", 115, {
			{ 0, "Off"}, {1, "Delay Mono"}, { 2, "DDL Stereo" }, { 3, "BBD Delay" }, { 4, "Chorus" },
			{ 5, "Phaser High" },{ 6, "Phaser Low" },{ 7, "Phase Mst" },{ 8, "Flanger 1" },{ 9, "Flanger 2" },
			{ 10, "Reverb" },{ 11, "Ring Mod" },{ 12, "Distortion" },{ 13, "HP Filter" }
		}),
		// TODO - really no values ?
		NrpnDefinition(156, 0, 255, "FX Param 1", 118),
		NrpnDefinition(157, 0, 127, "FX Param 2", 119),
		NrpnDefinition(158, 0, 1, "FX Clock Sync", 120),
		// TODO - really no values ?
		NrpnDefinition(163, 0, 2, "A/B Mode", 231),
		NrpnDefinition(164, 0, 1, "Seq Start/Stop", 137),
		// TODO - really no values ?
		NrpnDefinition(167, 0, 16, "Unison Detune", 208),
		NrpnDefinition(168, 0, 1, "Unison On/Off", 123),
		NrpnDefinition(169, 0, 16, "Unison Mode", 124),
		NrpnDefinition(170, 0, 5, "Keyboard Mode", 122),
		NrpnDefinition(171, 0, 120, "Split Point", 232),
		NrpnDefinition(172, 0, 1, "Arp On/Off", 136),
		NrpnDefinition(173, 0, 4, "Arp Mode", 132),
		NrpnDefinition(174, 0, 2, "Arp Octave", 133),
		NrpnDefinition(175, 0, 12, "Clock Divide", 131),
		// TODO - really no values ?
		NrpnDefinition(177, 0, 3, "Arp Repeats", 134),
		NrpnDefinition(178, 0, 1, "Arp Relatch", 135),
		NrpnDefinition(179, 30, 250, "BPM Tempo", 130),
		// TODO - really no values ?
		NrpnDefinition(182, 0, 4, "Gated Seq Mode", 138),
		NrpnDefinition(183, 0, 1, "Gated Seq On/Off", 137),
		NrpnDefinition(184, 0, 52, "Seq 1 Destination", 111),
		NrpnDefinition(185, 0, 53, "Seq 2 Destination (slew)", 112),
		NrpnDefinition(186, 0, 52, "Seq 3 Destination", 113),
		NrpnDefinition(187, 0, 53, "Seq 4 Destination (slew)", 114),
		// TODO - really no values ?
		NrpnDefinition(192, 207, 0, 127, "Gated Seq Track 1 Step 1,16", 140), // 126 is Reset, 127 is the Rest (Rest only on Track 1)
		NrpnDefinition(208, 223, 0, 126, "Gated Seq Track 2 Step 1,16", 156), // 126 is Reset
		NrpnDefinition(224, 239, 0, 126, "Gated Seq Track 3 Step 1,16", 172), // 126 is Reset
		NrpnDefinition(240, 255, 0, 126, "Gated Seq Track 4 Step 1,16", 188), // 126 is Reset
		// TODO - really no values ?
		NrpnDefinition(276, 339, 0, 127, "Seq Step 1,64 Note 1", 256),
		NrpnDefinition(340, 403, 128, 255, "Seq Step 1,6 Velocity 1", 320),
		NrpnDefinition(404, 467, 0, 127, "Seq Step 1,64 Note 2", 384),
		NrpnDefinition(468, 531, 128, 255, "Seq Step 1,64 Velocity 2", 448),
		NrpnDefinition(532, 595, 0, 127, "Seq Step 1,64 Note 3", 512),
		NrpnDefinition(596, 659, 128, 255, "Seq Step 1,64 Velocity 3", 576),
		NrpnDefinition(660, 723, 0, 127, "Seq Step 1,64 Note 4", 640),
		NrpnDefinition(724, 787, 128, 255, "Seq Step 1,64 Velocity 4", 704),
		NrpnDefinition(788, 851, 0, 127, "Seq Step 1,64 Note 5", 768),
		NrpnDefinition(852, 915, 128, 255, "Seq Step 1,64 Velocity 5", 832),
		NrpnDefinition(916, 979, 0, 127, "Seq Step 1,64 Note 6", 896),
		NrpnDefinition(980, 1043, 128, 255, "Seq Step 1,64 Velocity 6", 960)
	};


	std::string Rev2PatchNumber::friendlyName() const {
		// The Rev2 has 8 banks of 128 patches, in two sections U and F called U1 to U4 and F1 to F4
		int section = midiProgramNumber().toZeroBased() / 128;
		int program = midiProgramNumber().toZeroBased() % 128;
		return (boost::format("%s%d P%d") % (section == 0 ? "U" : "F") % ((bank_ % 4) + 1) % program).str();
	}

	Rev2Patch::Rev2Patch() : oscAllocated_(1)
	{
		// Load the init patch
		MidiMessage initPatch = MidiMessage(Rev2_InitPatch_syx, Rev2_InitPatch_syx_size);

		// Set data
		Rev2 rev2;
		auto initpatch = rev2.patchFromSysex(initPatch);
		setData(initpatch->data());
	}

	Rev2Patch::Rev2Patch(Synth::PatchData const &patchData) : Patch(patchData)
	{
	}

	std::string Rev2Patch::patchName() const
	{
		std::string layerA = layerName(0);
		std::string layerB = layerName(1);
		boost::trim(layerA);
		boost::trim(layerB);

		if (layerA == layerB) {
			switch (layerMode()) {
			case LayeredPatch::SEPARATE: return layerA + " [2x]"; // That's a weird state
			case LayeredPatch::STACK: return layerA + "[+]";
			case LayeredPatch::SPLIT: return layerA + "[|]"; // That's a weird state
			}
		}
		else {
			switch (layerMode()) {
			case LayeredPatch::SEPARATE: return layerA + "." + layerB;
			case LayeredPatch::STACK: return layerA + "[+]";  // return layerA + "+" + layerB;
			case LayeredPatch::SPLIT: return layerA + "|" + layerB;
			}
		}
		return "invalid patch";
	}

	void Rev2Patch::setName(std::string const &name)
	{
		ignoreUnused(name);
		jassert(false);
	}

	std::shared_ptr<PatchNumber> Rev2Patch::patchNumber() const {
		return std::make_shared<Rev2PatchNumber>(number_);
	}

	void Rev2Patch::setPatchNumber(MidiProgramNumber patchNumber) {
		number_ = Rev2PatchNumber(patchNumber);
	}

	int Rev2Patch::value(SynthParameterDefinition const &param) const
	{
		ignoreUnused(param);
		throw std::logic_error("The method or operation is not implemented.");
	}

	SynthParameterDefinition const & Rev2Patch::paramBySysexIndex(int sysexIndex) const
	{
		ignoreUnused(sysexIndex);
		throw std::logic_error("The method or operation is not implemented.");
	}

	void Rev2Patch::addNrpns(std::vector<NRPNValue> const &values)
	{
		// This will poke the values into the patch no matter what!
		for (auto value : values) {
			if (value.def().sysexIndex() != -1) {
				setAt(value.def().sysexIndex(), (uint8)value.value());
			}
			else {
				throw new std::runtime_error("Error - this is not a sysex parameter");
			}
		}
	}

/*	int Rev2Patch::addOsc2Patch(SimpleOsc const &osc)
	{
		std::vector<NRPNValue> result;

		// Set the Shape
		int targetOscNo = oscAllocated_++;
		if (targetOscNo > 2) {
			return targetOscNo;
		}
		jassert(targetOscNo >= 1 && targetOscNo <= 2);
		int shape = 0;
		int shapemod = 0;
		NrpnDefinition oscShape = Rev2::nrpn((boost::format("Osc %d Shape") % targetOscNo).str());
		if (osc.shape() == "Square") {
			// Square is a special case - the Rev2 will make that with Pulse and Shapemod == 50
			shape = oscShape.valueFromText("Pulse");
			shapemod = 50;
		}
		else if (osc.shape() == "Saw") {
			shape = oscShape.valueFromText("Saw");
		}
		else if (osc.shape() == "Triangle") {
			shape = oscShape.valueFromText("Triangle");
		}
		else if (osc.shape() == "Sawwave") {
			shape = oscShape.valueFromText("Saw+Triangle"); // Absolutely not sure if this is correct, need to measure with oscilloscope
			shapemod = osc.shapemod();
		}
		else if (osc.shape() == "Pulse") {
			shape = oscShape.valueFromText("Pulse");
			shapemod = osc.shapemod();
		}
		else if (osc.shape() == "") {
			shape = oscShape.valueFromText("Off");
		}
		else {
			jassert(false);
		}
		result.push_back(NRPNValue(oscShape, shape));
		result.push_back(NRPNValue(Rev2::nrpn((boost::format("Osc %d Pulse Width") % targetOscNo).str()), shapemod));

		// Set the frequency - The Rev2 will set C3 as 0, and semitones up and down from there. 
		result.push_back(NRPNValue(Rev2::nrpn((boost::format("Osc %d Freq") % targetOscNo).str()), 24 + osc.frequency()));

		// Now, to the specials! It is here where it gets interesting
		for (auto special : osc.specials()) {
			if (special->name() == "Noise") {
				if (targetOscNo == 1) {
					// Lucky us, our osc 1 has noise. 
					auto valueParam = std::dynamic_pointer_cast<ValueParameter>(special);
					if (valueParam) {
						// The Noise even is quantified, just as for the Rev
						result.push_back(NRPNValue(Rev2::nrpn("Noise"), valueParam->value()));
					}
					else {
						// Noise seems to be a flag that can just be turned on and off. Wild guess on how much noise you want
						result.push_back(NRPNValue(Rev2::nrpn("Noise"), 50));
					}
					if (targetOscNo != 1) {
						warnings_.push_back((boost::format("Can't turn on noise for oscillator %d, turned it on for Osc1") % targetOscNo).str());
					}
				}
			}
			else if (special->name() == "Detune") {
				auto valueParam = std::dynamic_pointer_cast<ValueParameter>(special);
				if (valueParam) {
					// That's easily possible with our Rev2
					result.push_back(NRPNValue(Rev2::nrpn((boost::format("Osc %d Freq Fine") % targetOscNo).str()), valueParam->value() * 50 / 32 + 50));
				}
				else {
					warnings_.push_back("Found Detune without value, ignoring that");
				}
			}
			else if (special->name() == "Click") {
				warnings_.push_back("Rev2 doesn't support click feature on oscillators");
			}
			else {
				warnings_.push_back((boost::format("Unknown special '%s' ignored") % special->name()).str());
			}
		}

		// Done, we collected all Nrpns we want to set in the patch for this Oscillator, so patch it into the patch!
		addNrpns(result);

		return targetOscNo;
	}

	void Rev2Patch::addEnv2Patch(Envelope const &env, int targetEnvNo)
	{
		std::vector<NRPNValue> result;

		std::string envName;
		switch (targetEnvNo) {
		case 0:
			envName = "VCA Env";
			break;
		case 1:
			envName = "Filter Env";
			break;
		case 2:
			envName = "Env 3";
			break;
		default:
			warnings_.push_back((boost::format("Ignoring definition for additional envelope #%d") % targetEnvNo).str());
			return;
		}

		// Ok, apart from the calibration, this should be straight forward?
		result.push_back(NRPNValue(Rev2::nrpn(envName + " Delay"), roundToInt(env.delay().percentage() * 127)));
		result.push_back(NRPNValue(Rev2::nrpn(envName + " Attack"), roundToInt(env.attack().percentage() * 127)));
		result.push_back(NRPNValue(Rev2::nrpn(envName + " Decay"), roundToInt(env.decay().percentage() * 127)));
		result.push_back(NRPNValue(Rev2::nrpn(envName + " Sustain"), roundToInt(env.sustain().percentage() * 127)));
		result.push_back(NRPNValue(Rev2::nrpn(envName + " Release"), roundToInt(env.release().percentage() * 127)));

		addNrpns(result);
	}*/

	std::vector<SynthParameterDefinition *> Rev2Patch::allParameterDefinitions()
	{
		return std::vector<SynthParameterDefinition *>();
	}

	LayeredPatch::LayerMode Rev2Patch::layerMode() const
	{
		//TODO - the sysex index should be taken from the Rev2 Parameter definitions
		switch (at(231)) {
		case 0: return LayeredPatch::SEPARATE;
		case 1: return LayeredPatch::STACK;
		case 2: return LayeredPatch::SPLIT;
		}
		throw std::runtime_error("Invalid layer mode of Rev2");
	}

	int Rev2Patch::numberOfLayers() const
	{
		// The Rev2 always has 2 layers, one might be an init patch, but we wouldn't know?
		return 2;
	}

	std::string Rev2Patch::layerName(int layerNo) const
	{
		// The Rev2 has a 20 character patch name storage for each of the 2 layers...	
		jassert(layerNo >= 0 && layerNo < numberOfLayers());
		size_t baseIndex = layerNo == 0 ? 235 : 1259; // Layer A starts at 235, Layer B starts at 1259
		std::string layerName;
		for (size_t i = baseIndex; i < baseIndex + 20; i++) {
			layerName.push_back(data()[i]);
		}
		return layerName;
	}

	std::shared_ptr<Rev2ParamDefinition> Rev2Patch::find(std::string const &paramID)
	{
		return std::make_shared<Rev2ParamDefinition>(nrpn(paramID));
	}

	std::vector<std::string> Rev2Patch::warnings()
	{
		return warnings_;
	}

	Rev2ParamDefinition::Rev2ParamDefinition(NrpnDefinition const &nrpn) : nrpn_(nrpn)
	{
	}

	std::string Rev2ParamDefinition::name() const
	{
		return nrpn_.name();
	}

	std::string Rev2ParamDefinition::valueAsText(int value) const
	{
		return nrpn_.valueAsText(value);
	}

	int Rev2ParamDefinition::sysexIndex() const
	{
		return nrpn_.sysexIndex();
	}

	int Rev2ParamDefinition::endSysexIndex() const
	{
		return nrpn_.sysexIndex() + nrpn_.numberOfValues() - 1;
	}

	std::string Rev2ParamDefinition::description() const
	{
		return nrpn_.name();
	}

	bool Rev2ParamDefinition::matchesController(int controllerNumber) const
	{
		return nrpn_.matchesController(controllerNumber);
	}

	int Rev2ParamDefinition::minValue() const
	{
		return nrpn_.min();
	}

	int Rev2ParamDefinition::maxValue() const
	{
		return nrpn_.max();
	}

	bool Rev2ParamDefinition::isActive(Patch const *patch) const
	{
		ignoreUnused(patch);
		jassert(false); // Not implemented yet
		return true;
	}

	bool Rev2ParamDefinition::valueInPatch(Patch const &patch, int &outValue) const
	{
		outValue = patch.at(sysexIndex());
		return true;
	}

	bool Rev2ParamDefinition::valueInPatch(Patch const &patch, std::vector<int> &outValue) const
	{
		if (type() != SynthParameterDefinition::ParamType::INT_ARRAY) {
			return false;
		}

		outValue.clear();
		for (int i = sysexIndex(); i <= endSysexIndex(); i++) {
			outValue.push_back(patch.at(i));
		}

		return true;
	}

	MidiBuffer Rev2ParamDefinition::setValueMessage(Patch const &patch, Synth *synth) const
	{
		int value;
		if (valueInPatch(patch, value)) {
			return MidiRPNGenerator::generate(synth->channel().toOneBasedInt(), nrpn_.number(), value, true);
		}
		else {
			return MidiBuffer();
		}
	}

	void Rev2ParamDefinition::setInPatch(Patch &patch, int value) const
	{
		jassert(type() == SynthParameterDefinition::ParamType::INT);
		patch.setAt(sysexIndex(), (uint8) value);
	}

	void Rev2ParamDefinition::setInPatch(Patch &patch, std::vector<int> value) const
	{
		int read = 0;
		for (int i = sysexIndex(); i <= endSysexIndex(); i++) {
			if (read < value.size()) {
				patch.setAt(i, (uint8) value[read++]);
			}
			else {
				// We just ignore additional bytes specified
			}
		}
	}

	midikraft::SynthParameterDefinition::ParamType Rev2ParamDefinition::type() const
	{
		if (sysexIndex() != endSysexIndex()) return SynthParameterDefinition::ParamType::INT_ARRAY;
		return SynthParameterDefinition::ParamType::INT;
	}

	NrpnDefinition Rev2Patch::nrpn(std::string const &name)
	{
		for (auto n : nrpns) {
			if (n.name() == name) {
				return n;
			}
		}
		throw new std::runtime_error("Unknown nrpn");
	}

	NrpnDefinition Rev2Patch::nrpn(int nrpnNumber)
	{
		// Layer B has the identical parameters as Layer A, so you will get back the definition for Layer A
		if (nrpnNumber >= 2048)
			nrpnNumber -= 2048;
		for (auto n : nrpns) {
			if (n.isOneOfThese(nrpnNumber)) {
				return n;
			}
		}
		throw new std::runtime_error("Unknown nrpn");
	}

	std::string Rev2Patch::nameOfNrpn(Rev2Message const &message)
	{
		std::string name = "Layer A ";
		int controller = message.nrpnController();
		if (controller >= 2048 && controller < 4096) {
			// This is a layer B NRPN, just subtract 2048 for now
			controller -= 2048;
			name = "Layer B ";
		}
		for (auto nrpn : nrpns) {
			if (nrpn.matchesController(controller)) {
				return name + nrpn.name();
			}
		}
		return name + "unknown nrpn";
	}

	int Rev2Patch::valueOfNrpnInPatch(Rev2Message const &message, Synth::PatchData const &patch) {
		// This works when our nrpn definition has a sysex index specified
		for (auto n : nrpns) {
			if (n.matchesController(message.nrpnController())) {
				return valueOfNrpnInPatch(n, patch);
			}
		}
		return 0;
	}

	int Rev2Patch::valueOfNrpnInPatch(NrpnDefinition const &nrpn, Synth::PatchData const &patch) {
		int index = nrpn.sysexIndex();
		if (index != -1) {
			if (index >= 0 && index < patch.size()) {
				return patch.at(nrpn.sysexIndex());
			}
		}
		// Invalid value for patch
		return 0;
	}



}
