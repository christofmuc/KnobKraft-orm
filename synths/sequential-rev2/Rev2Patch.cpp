/*
   Copyright (c) 2019 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "Rev2Patch.h"

#include "Sysex.h"
#include "Rev2.h"

#include "BinaryResources.h"

#include "MidiNote.h"

#include <boost/format.hpp>
#include <boost/algorithm/string.hpp>

namespace midikraft {

	std::map<int, std::string> kLfoShape = {
		{ { 0, "Triangle" },{ 1, "Sawtooth" },{ 2, "Rev Saw" },{ 3, "Square" },{ 4, "Random" } }
	};

	std::map<int, std::string> kLfoDestinations = {
		{ 0, "Off" }, { 1, "Osc1 Freq" },{ 2, "Osc2 Freq" },{ 3, "OscAll Freq" }, { 4, "Osc Mix" } 
		, { 5, "Noise" }, { 6, "Sub" }, { 7, "Osc1 Shape" }, { 8, "Osc2 Shape" }, { 9, "OscAll Shap" }
		, {10, "Cutoff" }, {11, "Res" }, {12, "AudioMod" }, {13, "VCA" }, {14, "Pan" }, {15, "LFO1 Freq" }
		, {16, "LFO2 Freq" }, {17, "LFO3 Freq" }, {18, "LFO4 Freq" }, {19, "LFOAll Frq" }
		, {20, "LFO1 Amt" }, {21, "LFO2 Amt" }, {22, "LFO3 Amt" }, {23, "LFO4 Amt" }, {24, "LFOAll Amt" }
		, {25, "LP Env Amt" }, {26, "VcaEnv Amt" }, {27, "Env3 Amt" }, {28, "EnvAll Amt" }
		, {29, "LPF Att" }, {30, "VCA Att" }, {31, "Env3 Att" }, {32, "EnvAll Att" }
		, {33, "LPF Dec" }, {34, "VCA Dec" }, {35, "Env3 Dec" }, {36, "EnvAll Dec" }
		, {37, "LPF Rel" }, {38, "VCA Rel" }, {39, "Env3 Rel" }, {40, "EnvAll Rel" }
		, {41, "Mod1 Amt"}, {42, "Mod2 Amt"}, {43, "Mod3 Amt"}, {44, "Mod4 Amt"}, {45, "Mod5 Amt"}, {46, "Mod6 Amt"}, {47, "Mod7 Amt"}, {48, "Mod8 Amt"}
		, {49, "Osc Slop"}, {50, "FX Mix"}, {51, "FX Param 1"}, {52, "FX Param 2"}, {53, "Seq Slew" } //The 53 is actually only available on Seq2 and Seq4 destinations!
	};

	std::map<int, std::string> kModSources = {
		{ 0, "Off" }, { 1, "Seq1" },{ 2, "Seq2" },{ 3, "Seq3" }, { 4, "Seq4" }
		, { 5, "LFO1" }, { 6, "LFO2" }, { 7, "LFO3" }, { 8, "LFO4" }, { 9, "Env LPF" }
		, {10, "Env VCA" }, {11, "Env 3" }, {12, "PitchBnd" }, {13, "ModWheel" }, {14, "Pressure" }, {15, "Breath" }
		, {16, "Foot" }, {17, "Expressn" }, {18, "Velocity" }, {19, "Note Num" }
		, {20, "Noise" }, {21, "DC" }, {22, "Audio Out" }
	};

	std::function<std::string(int)> noteNumberToName = [](int value) { return MidiNote(value).name(); };

	std::vector<Rev2ParamDefinition> nrpns = {
		Rev2ParamDefinition(0, 0, 120, "Osc 1 Freq", 0, noteNumberToName),
		Rev2ParamDefinition(1, 0, 100, "Osc 1 Freq Fine", 2),
		Rev2ParamDefinition(2, 0, 4, "Osc 1 Shape Mod", 4, { {0, "Off"}, { 1, "Saw" }, { 2, "Saw+Triangle"}, { 3, "Triangle" }, { 4, "Pulse" } }),
		Rev2ParamDefinition(3, 0, 127, "Osc 1 Glide", 8),
		Rev2ParamDefinition(4, 0, 1, "Osc 1 Key On/Off", 10),
		Rev2ParamDefinition(5, 0, 120, "Osc 2 Freq", 1, noteNumberToName),
		Rev2ParamDefinition(6, 0, 100, "Osc 2 Freq Fine", 3),
		Rev2ParamDefinition(7, 0, 4, "Osc 2 Shape Mod", 5, { { 0, "Off" },{ 1, "Saw" },{ 2, "Saw+Triangle" },{ 3, "Triangle" },{ 4, "Pulse" } }),
		Rev2ParamDefinition(8, 0, 127, "Osc 2 Glide", 9),
		Rev2ParamDefinition(9, 0, 1, "Osc. 2 Key On/Off", 11),
		Rev2ParamDefinition(10, 0, 1, "Sync On/Off", 17),
		Rev2ParamDefinition(11, 0, 3, "Glide Mode", 18, { { 0, "Fixed Rate" },{ 1, "Fixed Rate A" },{ 2, "Fixed Time" },{ 3, "Fixed Time A" } }),
		Rev2ParamDefinition(12, 0, 127, "Osc Slop", 21),
		Rev2ParamDefinition(13, 0, 127, "Osc 1/2 Mix", 14),
		Rev2ParamDefinition(14, 0, 127, "Noise Level", 16),
		Rev2ParamDefinition(15, 0, 164, "Cutoff", 22),
		Rev2ParamDefinition(16, 0, 127, "Resonance", 23),
		Rev2ParamDefinition(17, 0, 127, "LPF Key Amt", 24),
		Rev2ParamDefinition(18, 0, 127, "LPF Audio Mod", 25),
		Rev2ParamDefinition(19, 0, 1, "2 pole/4 pole mode", 26, { {0, "2 pole 12db" }, { 1, "4 pole 24db" } }),
		Rev2ParamDefinition(20, 0, 254, "Env LPF Amt", 32),
		Rev2ParamDefinition(21, 0, 127, "Env LPF Vel Amt", 35),
		Rev2ParamDefinition(22, 0, 127, "Env LPF Delay", 38),
		Rev2ParamDefinition(23, 0, 127, "Env LPF Attack", 41),
		Rev2ParamDefinition(24, 0, 127, "Env LPF Decay", 44),
		Rev2ParamDefinition(25, 0, 127, "Env LPF Sustain", 47),
		Rev2ParamDefinition(26, 0, 127, "Env LPF Release", 50),
		// 27 is really empty.If you try to set this, you get a change in index #33
		Rev2ParamDefinition(28, 0, 127, "Pan Spread", 29),
		Rev2ParamDefinition(29, 0, 127, "Program Volume", 28),
		Rev2ParamDefinition(30, 0, 127, "Env VCA Amt", 33),
		Rev2ParamDefinition(31, 0, 127, "Env VCA Vel Amt", 36),
		Rev2ParamDefinition(32, 0, 127, "Env VCA Delay", 39),
		Rev2ParamDefinition(33, 0, 127, "Env VCA Attack", 42),
		Rev2ParamDefinition(34, 0, 127, "Env VCA Decay", 45),
		Rev2ParamDefinition(35, 0, 127, "Env VCA Sustain", 48),
		Rev2ParamDefinition(36, 0, 127, "Env VCA Release", 51),
		Rev2ParamDefinition(37, 0, 127, "LFO 1 Freq", 53),
		Rev2ParamDefinition(38, 0, 4, "LFO 1 Shape", 57, kLfoShape),
		Rev2ParamDefinition(39, 0, 127, "LFO 1 Amt", 61),
		Rev2ParamDefinition(40, 0, 52, "LFO 1 Dest", 65, kLfoDestinations),
		Rev2ParamDefinition(41, 0, 1, "LFO 1 Clock Sync", 69),
		Rev2ParamDefinition(42, 0, 150, "LFO 2 Freq", 54),
		Rev2ParamDefinition(43, 0, 4, "LFO 2 Shape", 58, kLfoShape),
		Rev2ParamDefinition(44, 0, 127, "LFO 2 Amt", 62),
		Rev2ParamDefinition(45, 0, 52, "LFO 2 Dest", 66, kLfoDestinations),
		Rev2ParamDefinition(46, 0, 1, "LFO 2 Clock Sync", 70),
		Rev2ParamDefinition(47, 0, 150, "LFO 3 Freq", 55),
		Rev2ParamDefinition(48, 0, 4, "LFO 3 Shape", 59, kLfoShape),
		Rev2ParamDefinition(49, 0, 127, "LFO 3 Amt", 63),
		Rev2ParamDefinition(50, 0, 52, "LFO 3 Dest", 67, kLfoDestinations),
		Rev2ParamDefinition(51, 0, 1, "LFO 3 Clock Sync", 71),
		Rev2ParamDefinition(52, 0, 150, "LFO 4 Freq", 56),
		Rev2ParamDefinition(53, 0, 4, "LFO 4 Shape", 60, kLfoShape),
		Rev2ParamDefinition(54, 0, 127, "LFO 4 Amt", 64),
		Rev2ParamDefinition(55, 0, 52, "LFO 4 Dest", 68, kLfoDestinations),
		Rev2ParamDefinition(56, 0, 1, "LFO 4 Clock Sync", 72),
		Rev2ParamDefinition(57, 0, 52, "Env 3 Dest", 30, kLfoDestinations),
		Rev2ParamDefinition(58, 0, 254, "Env 3 Amount", 34),
		Rev2ParamDefinition(59, 0, 127, "Env 3 Vel Amt", 37),
		Rev2ParamDefinition(60, 0, 127, "Env 3 Delay", 40),
		Rev2ParamDefinition(61, 0, 127, "Env 3 Attack", 43),
		Rev2ParamDefinition(62, 0, 127, "Env 3 Decay", 46),
		Rev2ParamDefinition(63, 0, 127, "Env 3 Sustain", 49),
		Rev2ParamDefinition(64, 0, 127, "Env 3 Release", 52),
		Rev2ParamDefinition(65, 0, 22, "Mod 1 Source", 77, kModSources),
		Rev2ParamDefinition(66, 0, 254, "Mod 1 Amount", 85),
		Rev2ParamDefinition(67, 0, 52, "Mod 1 Dest", 93, kLfoDestinations),
		Rev2ParamDefinition(68, 0, 22, "Mod 2 Source", 78, kModSources),
		Rev2ParamDefinition(69, 0, 254, "Mod 2 Amount", 86),
		Rev2ParamDefinition(70, 0, 52, "Mod 2 Dest", 94, kLfoDestinations),
		Rev2ParamDefinition(71, 0, 22, "Mod 3 Source", 79, kModSources),
		Rev2ParamDefinition(72, 0, 254, "Mode 3 Amount", 87),
		Rev2ParamDefinition(73, 0, 52, "Mode 3 Dest", 95, kLfoDestinations),
		Rev2ParamDefinition(74, 0, 22, "Mod 4 Source", 80, kModSources),
		Rev2ParamDefinition(75, 0, 254, "Mod 4 Amount", 88),
		Rev2ParamDefinition(76, 0, 52, "Mod 4 Dest", 96, kLfoDestinations),
		Rev2ParamDefinition(77, 0, 22, "Mod 5 Source", 81, kModSources),
		Rev2ParamDefinition(78, 0, 254, "Mod 5 Amount", 89),
		Rev2ParamDefinition(79, 0, 52, "Mod 5 Dest", 97, kLfoDestinations),
		Rev2ParamDefinition(80, 0, 22, "Mod 6 Source", 82, kModSources),
		Rev2ParamDefinition(81, 0, 254, "Mod 6 Amount", 90),
		Rev2ParamDefinition(82, 0, 52, "Mod 6 Dest", 98, kLfoDestinations),
		Rev2ParamDefinition(83, 0, 22, "Mod 7 Source", 83, kModSources),
		Rev2ParamDefinition(84, 0, 254, "Mod 7 Amount", 91),
		Rev2ParamDefinition(85, 0, 52, "Mod 7 Dest", 99, kLfoDestinations),
		Rev2ParamDefinition(86, 0, 22, "Mod 8 Source", 84, kModSources),
		Rev2ParamDefinition(87, 0, 254, "Mod 8 Amount", 92),
		Rev2ParamDefinition(88, 0, 52, "Mod 8 Dest", 100, kLfoDestinations),
		// TODO - really no values ?
		Rev2ParamDefinition(97, 0, 1, "Env 3 Repeat", 31),
		Rev2ParamDefinition(98, 0, 127, "VCA Level", 27),
		Rev2ParamDefinition(99, 0, 1, "Osc 1 Note Reset", 12),
		// TODO - really no values ?
		Rev2ParamDefinition(102, 0, 99, "Osc 1 Pulse Width", 6),
		Rev2ParamDefinition(103, 0, 99, "Osc 2 Pulse Width", 7),
		Rev2ParamDefinition(104, 0, 1, "Osc 2 Note Reset", 13),
		Rev2ParamDefinition(105, 0, 1, "LFO 1 Key Sync", 73),
		Rev2ParamDefinition(106, 0, 1, "LFO 2 Key Sync", 74),
		Rev2ParamDefinition(107, 0, 1, "LFO 3 Key Sync", 75),
		Rev2ParamDefinition(108, 0, 1, "LFO 4 Key Sync", 76),
		// TODO - really no values ?
		Rev2ParamDefinition(110, 0, 127, "Sub Level", 15),
		Rev2ParamDefinition(111, 0, 1, "Glide On/Off", 19),
		// TODO - really no values ?
		Rev2ParamDefinition(113, 0, 12, "Pitch Bend Range", 20),
		Rev2ParamDefinition(114, 0, 1, "Pan Mod Mode", 209, { { 0, "Alternate" }, { 1, "Fixed" } }),
		// TODO - really no values ?
		Rev2ParamDefinition(116, 0, 254, "Mod Wheel Amount", 101),
		Rev2ParamDefinition(117, 0, 52, "Mod Wheel Dest", 102, kLfoDestinations),
		Rev2ParamDefinition(118, 0, 254, "Pressure Amount", 103),
		Rev2ParamDefinition(119, 0, 52, "Pressure Dest", 104, kLfoDestinations),
		Rev2ParamDefinition(120, 0, 254, "Breath Amount", 105),
		Rev2ParamDefinition(121, 0, 52, "Breath Dest", 106, kLfoDestinations),
		Rev2ParamDefinition(122, 0, 254, "Velocity Amount", 107),
		Rev2ParamDefinition(123, 0, 52, "Velocity Dest", 108, kLfoDestinations),
		Rev2ParamDefinition(124, 0, 254, "Foot Ctrl Amount", 109),
		Rev2ParamDefinition(125, 0, 52, "Foot Ctrl Dest", 110, kLfoDestinations),
		// TODO - really no values ?
		Rev2ParamDefinition(153, 0, 1, "FX On/Off", 116),
		Rev2ParamDefinition(154, 0, 13, "FX Type", 115, {
			{ 0, "Off"}, {1, "Delay Mono"}, { 2, "DDL Stereo" }, { 3, "BBD Delay" }, { 4, "Chorus" },
			{ 5, "Phaser High" },{ 6, "Phaser Low" },{ 7, "Phase Mst" },{ 8, "Flanger 1" },{ 9, "Flanger 2" },
			{ 10, "Reverb" },{ 11, "Ring Mod" },{ 12, "Distortion" },{ 13, "HP Filter" }
		}),
		Rev2ParamDefinition(155, 0, 127, "FX Mix", 117),
		Rev2ParamDefinition(156, 0, 255, "FX Param 1", 118),
		Rev2ParamDefinition(157, 0, 127, "FX Param 2", 119),
		Rev2ParamDefinition(158, 0, 1, "FX Clock Sync", 120),
		// TODO - really no values ?
		Rev2ParamDefinition(163, 0, 2, "A/B Mode", 231, { { 0, "Single Layer" }, { 1, "Stacked" }, { 2, "Split" } }),
		Rev2ParamDefinition(164, 0, 1, "Poly Seq Start/Stop", 137),
		// TODO - really no values ?
		Rev2ParamDefinition(167, 0, 16, "Unison Detune", 208),
		Rev2ParamDefinition(168, 0, 1, "Unison On/Off", 123),
		Rev2ParamDefinition(169, 0, 16, "Unison Mode", 124),
		Rev2ParamDefinition(170, 0, 5, "Key Mode", 122, { { 0, "Low" }, { 1, "Hi" }, { 2, "Last" }, { 3, "LowR" }, { 4, "HiR"}, {5, "LastR"} }),
		Rev2ParamDefinition(171, 0, 120, "Split Point", 232, noteNumberToName),
		Rev2ParamDefinition(172, 0, 1, "Arp On/Off", 136),
		Rev2ParamDefinition(173, 0, 4, "Arp Mode", 132, { { 0, "Up" }, { 1, "Down"}, { 2, "Up+Down" }, { 3, "Random" },  { 4, "Assign" } }),
		Rev2ParamDefinition(174, 0, 2, "Arp Octave", 133),
		Rev2ParamDefinition(175, 0, 12, "Clock Divide", 131, { {0, "Half" }, { 1, "Quarter"}, { 2, "8th" }, { 3, "8 Half"}, { 4, "8 Swing" } , {5, "8 Trip"}
			, { 6, "16th" }, { 7, "16 Half"}, { 8, "16 Swing" } , {9, "16 Trip"}, { 10, "32nd" }, { 11, "32nd Trip"}, { 12, "64 Trip" } }),
		// TODO - really no values ?
		Rev2ParamDefinition(177, 0, 3, "Arp Repeats", 134),
		Rev2ParamDefinition(178, 0, 1, "Arp Relatch", 135),
		Rev2ParamDefinition(179, 30, 250, "BPM Tempo", 130),
		// TODO - really no values ?
		Rev2ParamDefinition(182, 0, 4, "Gated Seq Mode", 138, { {0, "Normal"}, { 1, "No Reset"}, { 2, "No Gate"}, { 3, "No G/R"}, {4, "Key Step"} }),
		Rev2ParamDefinition(183, 0, 1, "Seq Mode", 137, { {0, "Gated"}, { 1, "Poly"} }),
		Rev2ParamDefinition(184, 0, 52, "Seq 1 Dest", 111, kLfoDestinations),
		Rev2ParamDefinition(185, 0, 53, "Seq 2 Dest", 112, kLfoDestinations),
		Rev2ParamDefinition(186, 0, 52, "Seq 3 Dest", 113, kLfoDestinations),
		Rev2ParamDefinition(187, 0, 53, "Seq 4 Dest", 114, kLfoDestinations),
		// TODO - really no values ?
		Rev2ParamDefinition(192, 207, 0, 127, "Seq Track 1", 140), // 126 is Reset, 127 is the Rest (Rest only on Track 1)
		Rev2ParamDefinition(208, 223, 0, 126, "Seq Track 2", 156), // 126 is Reset
		Rev2ParamDefinition(224, 239, 0, 126, "Seq Track 3", 172), // 126 is Reset
		Rev2ParamDefinition(240, 255, 0, 126, "Seq Track 4", 188), // 126 is Reset
		// TODO - really no values ?
		Rev2ParamDefinition(276, 339, 0, 127, "Poly Seq Note 1", 256, noteNumberToName),
		Rev2ParamDefinition(340, 403, 128, 255, "Poly Seq Vel 1", 320),
		Rev2ParamDefinition(404, 467, 0, 127, "Poly Seq Note 2", 384, noteNumberToName),
		Rev2ParamDefinition(468, 531, 128, 255, "Poly Seq Vel 2", 448),
		Rev2ParamDefinition(532, 595, 0, 127, "Poly Seq Note 3", 512, noteNumberToName),
		Rev2ParamDefinition(596, 659, 128, 255, "Poly Seq Vel 3", 576),
		Rev2ParamDefinition(660, 723, 0, 127, "Poly Seq Note 4", 640, noteNumberToName),
		Rev2ParamDefinition(724, 787, 128, 255, "Poly Seq Vel 4", 704),
		Rev2ParamDefinition(788, 851, 0, 127, "Poly Seq Note 5", 768, noteNumberToName),
		Rev2ParamDefinition(852, 915, 128, 255, "Poly Seq Vel 5", 832),
		Rev2ParamDefinition(916, 979, 0, 127, "Poly Seq Note 6", 896, noteNumberToName),
		Rev2ParamDefinition(980, 1043, 128, 255, "Poly Seq Vel 6", 960)
	};

	std::string Rev2PatchNumber::friendlyName() const {
		// The Rev2 has 8 banks of 128 patches, in two sections U and F called U1 to U4 and F1 to F4
		int section = midiProgramNumber().toZeroBased() / 128;
		int program = midiProgramNumber().toZeroBased() % 128;
		return (boost::format("%s%d P%d") % (section == 0 ? "U" : "F") % ((bank_ % 4) + 1) % program).str();
	}

	Rev2Patch::Rev2Patch() : Patch(Rev2::PATCH)
	{
		// Load the init patch
		MidiMessage initPatch = MidiMessage(Rev2_InitPatch_syx, Rev2_InitPatch_syx_size);

		// Set data
		Rev2 rev2;
		auto initpatch = rev2.patchFromSysex(initPatch);
		setData(initpatch->data());
	}

	Rev2Patch::Rev2Patch(Synth::PatchData const &patchData) : Patch(Rev2::PATCH, patchData)
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

	std::vector<std::shared_ptr<SynthParameterDefinition>> Rev2Patch::allParameterDefinitions()
	{
		//TODO this will leak memory
		std::vector<std::shared_ptr<SynthParameterDefinition>> result;
		for (auto n : nrpns) {
			result.push_back(std::make_shared<Rev2ParamDefinition>(n));
		}
		return result;
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

	void Rev2Patch::setLayerName(int layerNo, std::string const &layerName)
	{
		jassert(layerNo >= 0 && layerNo < numberOfLayers());
		int baseIndex = layerNo == 0 ? 235 : 1259; // Layer A starts at 235, Layer B starts at 1259
		for (int i = 0; i < 20; i++) {
			if (i < (int) layerName.size()) {
				setAt(baseIndex + i, layerName[i]);
			}
			else {
				// Fill the 20 characters with space
				setAt(baseIndex + i, ' ');
			}
		}
	}

	std::shared_ptr<Rev2ParamDefinition> Rev2Patch::find(std::string const &paramID)
	{
		for (auto &n : nrpns) {
			if (n.name() == paramID) {
				return std::make_shared<Rev2ParamDefinition>(n); // Copy construct a shared ptr owned object, handed off to python 
			}
		}
		return nullptr;
	}

}
