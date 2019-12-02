#include "Rev2.h"

#include "Patch.h"

#include "Rev2Message.h"
#include "NrpnDefinition.h"
//#include "SynthSetup.h"

#include "Rev2Patch.h"
//#include "Rev2BCR2000.h"
//#include "BCR2000.h"

#include <algorithm>
#include <boost/format.hpp>

#include "MidiHelpers.h"

namespace midikraft {

	// Definitions for the SysEx format we need
	const size_t cGatedSeqOnIndex = 139;
	const size_t cGatedSeqDestination = 111;
	const size_t cGatedSeqIndex = 140;
	const size_t cStepSeqNote1Index = 256;
	const size_t cStepSeqVelocity1Index = 320;
	const size_t cLayerB = 2048 / 2;
	const size_t cABMode = 231;
	const size_t cBpmTempo = 130;
	const size_t cClockDivide = 131;

	// Some constants
	const uint8 cDefaultNote = 0x3c;

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

	std::vector<Range<int>> kRev2BlankOutZones = {
		{ 211, 231 }, // unused according to doc
		{ 1235, 1255 }, // same in layer B
		{ 235, 255 }, // name of layer A
		{ 1259, 1279 } // name of layer B
	};

	std::string intervalToText(int interval) {
		if (interval == 0) {
			return "same note";
		}
		int octave = 0;
		while (interval >= 12) {
			octave++;
			interval -= 12;
		}
		std::string octaveText = octave == 1 ? "one octave" : (boost::format("%d octaves") % octave).str();
		std::string semitoneText = interval == 0 ? "same note" : (boost::format("%d semi-tones") % interval).str();
		if (interval == 0) {
			return octaveText;
		}
		else if (octave == 0) {
			return semitoneText;
		}
		else {
			return octaveText + " and " + semitoneText;
		}
	}

	Rev2::Rev2() : DSISynth(0x2f /* Rev2 ID */)
	{
	}

/*	std::shared_ptr<Patch> Rev2::synthSetupToPatch(SynthSetup const &sound, std::function<void(std::string warning)> logWarning) {
		auto patch = std::make_shared<Rev2Patch>();

		// Start with the oscillator setup
		for (auto osc : sound.oscillators_) {
			auto compound = std::dynamic_pointer_cast<CompoundOsc>(osc);
			if (compound) {
				// Oh, problem - this synth has compound oscillators, that is, many in one. This is going to be tricky, because
				// the Rev2 can only do one waveform per oscillator.
				if (compound->osc1().isMakingSound()) {
					patch->addOsc2Patch(compound->osc1());
				}
				if (compound->osc2().isMakingSound()) {
					patch->addOsc2Patch(compound->osc2());
				}
			}
			else {
				// Then it must be simple!
				patch->addOsc2Patch(*std::dynamic_pointer_cast<SimpleOsc>(osc));
			}
		}
		// Set the mix - that value is not very well defined
		patch->addNrpns({ NRPNValue(nrpn("Osc Mix"), sound.mix_ * 128 / 64) });

		// Now, the envelopes
		for (auto env : sound.envelopes_) {
			int targetNo = 0;
			if (env->name() == "Env 1" || env->name() == Envelope::kFilter) {
				targetNo = 1;
			}
			else if (env->name() == "Env 2" || env->name() == Envelope::kVCA) {
				targetNo = 0;
			}
			else if (env->name() == "Env 3") {
				targetNo = 2;
			}
			else {
				throw std::runtime_error("Unknown envelope");
			}
			patch->addEnv2Patch(*env, targetNo);
		}

		return patch;
	}

	std::string Rev2::patchToText(PatchData const &patch) {
		std::string desc;

		// First, describe the oscillator setup!
		auto osc1shape = nrpn("Osc 1 Shape");
		auto osc1on = valueOfNrpnInPatch(osc1shape, patch) != 0;
		auto osc2shape = nrpn("Osc 2 Shape");
		auto osc2on = valueOfNrpnInPatch(osc2shape, patch) != 0;
		auto oscmix = nrpn("Osc Mix");
		auto osc1freq = nrpn("Osc 1 Freq");
		auto osc2freq = nrpn("Osc 2 Freq");
		if (osc1on && !osc2on) {
			desc = (boost::format("One %s osc") % osc1shape.valueAsText(valueOfNrpnInPatch(osc1shape, patch))).str();
		}
		else if (osc2on && !osc1on) {
			desc = (boost::format("One %s osc") % osc2shape.valueAsText(valueOfNrpnInPatch(osc2shape, patch))).str();
		}
		else if (osc2on && osc1on) {
			// Two oscillators turned on - then we are interested in the balance between the two
			auto oscmixval = valueOfNrpnInPatch(oscmix, patch);
			auto osc1perc = (1 - oscmixval / 127.0) * 100.0;
			auto osc2perc = oscmixval / 127.0 * 100.0;
			// And the interval
			auto osc1f = valueOfNrpnInPatch(osc1freq, patch);
			auto osc2f = valueOfNrpnInPatch(osc2freq, patch);
			std::string intervalText = "at the same note";
			if (osc2f < osc1f) {
				intervalText = (boost::format("%s lower") % intervalToText(osc1f - osc2f)).str();
			}
			else if (osc1f < osc2f) {
				intervalText = (boost::format("%s higher") % intervalToText(osc2f - osc1f)).str();
			}
			desc = (boost::format("osc1 is %s (%.0f%%) and osc2 is %s (%.0f%%) %s")
				% osc1shape.valueAsText(valueOfNrpnInPatch(osc1shape, patch)) % osc1perc
				% osc2shape.valueAsText(valueOfNrpnInPatch(osc2shape, patch)) % osc2perc
				% intervalText).str();
		}
		else {
			desc = "Oscillators are off";
		}

		// Effects section
		auto fxon = nrpn("FX On/Off");
		if (valueOfNrpnInPatch(fxon, patch) == 1) {
			auto fxselect = nrpn("FX Select");
			std::string effects = fxselect.valueAsText(valueOfNrpnInPatch(fxselect, patch));
			desc = desc + "\nEffect is " + effects;
		}

		return desc;
	}*/

	Synth::PatchData Rev2::filterVoiceRelevantData(PatchData const &unfilteredData) const
	{
		return Patch::blankOut(kRev2BlankOutZones, unfilteredData);
	}

	int Rev2::numberOfBanks() const
	{
		return 8;
	}

	int Rev2::numberOfPatches() const
	{
		return 128;
	}

	std::string Rev2::friendlyBankName(MidiBankNumber bankNo) const
	{
		int section = bankNo.toZeroBased() / 4;
		int bank = bankNo.toZeroBased();
		return (boost::format("%s%d") % (section == 0 ? "U" : "F") % ((bank % 4) + 1)).str();
	}

	std::shared_ptr<Patch> Rev2::patchFromSysex(const MidiMessage& message) const
	{
		int startIndex = -1;

		if (isEditBufferDump(message)) {
			startIndex = 3;
		}

		if (isSingleProgramDump(message)) {
			startIndex = 5;
		}

		if (startIndex == -1) {
			// ups
			jassert(false);
			return nullptr;
		}

		// Decode the data
		const uint8 *startOfData = &message.getSysExData()[startIndex];
		auto patchData = unescapeSysex(startOfData, message.getSysExDataSize() - startIndex, 2048);
		return std::make_shared<Rev2Patch>(patchData);
	}

	std::shared_ptr<Patch> Rev2::patchFromPatchData(const Synth::PatchData &data, std::string const &name, MidiProgramNumber place) const {
		ignoreUnused(name, place);
		return std::make_shared<Rev2Patch>(data);
	}


	std::vector<juce::MidiMessage> Rev2::patchToSysex(const Patch &patch) const
	{
		// By default, create an edit buffer dump file...
		std::vector<uint8> programEditBufferDataDump({ 0x01 /* DSI */, midiModelID_, 0x03 /* Edit Buffer Data */ });
		auto patchData = escapeSysex(patch.data());
		jassert(patch.data().size() == 2046);
		jassert(patchData.size() == 2339);
		std::copy(patchData.begin(), patchData.end(), std::back_inserter(programEditBufferDataDump));
		return std::vector<MidiMessage>({ MidiHelpers::sysexMessage(programEditBufferDataDump) });
	}

/*	SynthSetup Rev2::patchToSynthSetup(Synth::PatchData const &patch)
	{
		return SynthSetup("unknown");
	}*/

	uint8 Rev2::clamp(int value, uint8 minimum /* = 9*/, uint8 maximum /* = 127 */) {
		return static_cast<uint8>(std::min((int)maximum, std::max(value, (int)minimum)));
	}

	void Rev2::compareMessages(const MidiMessage &msg1, const MidiMessage &msg2) {
		auto data1 = msg1.getSysExData();
		auto data2 = msg2.getSysExData();
		for (int i = 0; i < std::min(msg1.getSysExDataSize(), msg2.getSysExDataSize()); i++) {
			if (data1[i] != data2[i]) {
				jassert(false);
			}
		}
		if (msg1.getSysExDataSize() != msg2.getSysExDataSize()) {
			jassert(false);
		}
	}

	juce::MidiMessage Rev2::filterProgramEditBuffer(const MidiMessage &programEditBuffer, std::function<void(std::vector<uint8> &)> filterExpressionInPlace) {
		if (!isEditBufferDump(programEditBuffer)) {
			jassert(false);
			return MidiMessage(); // Empty sysex message so it doesn't crash
		}

		// Decode the data
		const uint8 *startOfData = &programEditBuffer.getSysExData()[3];
		std::vector<uint8> programEditBufferDecoded = unescapeSysex(startOfData, programEditBuffer.getSysExDataSize() - 3, 2048);

		// Perform filter operation
		filterExpressionInPlace(programEditBufferDecoded);

		return buildSysexFromEditBuffer(programEditBufferDecoded);
	}

	juce::MidiMessage Rev2::buildSysexFromEditBuffer(std::vector<uint8> editBuffer) {
		// Done, now create a new encoded buffer
		std::vector<uint8> encodedBuffer = escapeSysex(editBuffer);

		// Build the sysex method with the patched buffer
		std::vector<uint8> sysEx({ 0b00000001, 0b00101111, 0b00000011 });
		sysEx.insert(sysEx.end(), encodedBuffer.begin(), encodedBuffer.end());
		auto result = MidiMessage::createSysExMessage(&sysEx.at(0), (int)sysEx.size());
		return result;
	}

	bool isPolySequencerRest(int note, int velocity) {
		// Wild guess...
		return note == 60 && velocity == 128;
	}

	bool isPolySequencerTie(int note, int velocity) {
		ignoreUnused(velocity);
		return note > 128;
	}

	juce::MidiMessage Rev2::patchPolySequenceToGatedTrack(const MidiMessage& message, int gatedSeqTrack)
	{
		return filterProgramEditBuffer(message, [gatedSeqTrack](std::vector<uint8> &programEditBuffer) {
			// Copy the PolySequence into the Gated Track
			// Find the lowest note in the poly sequence
			int lowestNote = 127;
			for (int i = 0; i < 16; i++) {
				if (programEditBuffer[cStepSeqNote1Index + i] < lowestNote) {
					lowestNote = programEditBuffer[cStepSeqNote1Index + i];
				}
			}

			// As the Gated Sequencer only has positive values, and I want the key to be the first key of the sequence, our only choice is
			// to move up a few octaves so we stay in key...
			int indexNote = programEditBuffer[cStepSeqNote1Index];
			while (lowestNote < indexNote) {
				indexNote -= 12;
			}

			for (int i = 0; i < 16; i++) {
				// 16 steps in the gated sequencer...
				// The gated sequencer allows half-half steps in pitch, so we multiply by 2...
				uint8 notePlayed = programEditBuffer[cStepSeqNote1Index + i];
				uint8 velocityPlayed = programEditBuffer[cStepSeqVelocity1Index + i];
				if (velocityPlayed > 0 && !isPolySequencerRest(notePlayed, velocityPlayed) && !isPolySequencerTie(notePlayed, velocityPlayed)) {
					programEditBuffer[gatedSeqTrack * 16 + i + cGatedSeqIndex] = clamp((notePlayed - indexNote) * 2, 0, 125);
				}
				else {
					// 126 is the reset in the gated sequencer, 127 is the rest, which is only allowed in track 1 if I believe the Prophet 8 documentation
					programEditBuffer[gatedSeqTrack * 16 + i + cGatedSeqIndex] = 127;
				}
				programEditBuffer[(gatedSeqTrack + 1) * 16 + i + cGatedSeqIndex] = clamp(velocityPlayed / 2, 0, 125);
			}

			// Poke the sequencer on and set the destination to OscAllFreq
			programEditBuffer[cGatedSeqOnIndex] = 0; // 0 is gated sequencer, 1 is poly sequencer
			programEditBuffer[cGatedSeqDestination] = 3;

			// If we are in a stacked program, we copy layer A to B so both sounds get the same sequence
			if (programEditBuffer[cABMode] == 1) {
				programEditBuffer[cLayerB + cGatedSeqDestination] = programEditBuffer[cGatedSeqDestination];
				programEditBuffer[cLayerB + cGatedSeqOnIndex] = programEditBuffer[cGatedSeqOnIndex];
				std::copy(std::next(programEditBuffer.begin(), cGatedSeqIndex),
					std::next(programEditBuffer.begin(), cGatedSeqIndex + 4 * 16),
					std::next(programEditBuffer.begin(), cLayerB + cGatedSeqIndex));

				// And we should make sure that the bpm and clock divide is the same on layer B
				programEditBuffer[cLayerB + cBpmTempo] = programEditBuffer[cBpmTempo];
				programEditBuffer[cLayerB + cClockDivide] = programEditBuffer[cClockDivide];
			}
		});
	}

	juce::MidiMessage Rev2::copySequencersFromOther(const MidiMessage& currentProgram, const MidiMessage &lockedProgram)
	{
		// Decode locked data as well
		jassert(isEditBufferDump(lockedProgram));
		const uint8 *startOfData = &lockedProgram.getSysExData()[3];
		std::vector<uint8> lockedProgramBufferDecoded = unescapeSysex(startOfData, lockedProgram.getSysExDataSize() - 3, 2048);
		return filterProgramEditBuffer(currentProgram, [lockedProgramBufferDecoded](std::vector<uint8> &programEditBuffer) {
			// Copy poly sequence of both layers, 6 tracks with 64 bytes for note and 64 bytes for velocity each!
			std::copy(std::next(lockedProgramBufferDecoded.begin(), cStepSeqNote1Index),
				std::next(lockedProgramBufferDecoded.begin(), cStepSeqNote1Index + 6 * 64 * 2),
				std::next(programEditBuffer.begin(), cStepSeqNote1Index));
			std::copy(std::next(lockedProgramBufferDecoded.begin(), cLayerB + cStepSeqNote1Index),
				std::next(lockedProgramBufferDecoded.begin(), cLayerB + cStepSeqNote1Index + 6 * 64 * 2),
				std::next(programEditBuffer.begin(), cLayerB + cStepSeqNote1Index));

			// Copy 4 tracks with 16 bytes each for the gated sequencer
			std::copy(std::next(lockedProgramBufferDecoded.begin(), cGatedSeqIndex),
				std::next(lockedProgramBufferDecoded.begin(), cGatedSeqIndex + 4 * 16),
				std::next(programEditBuffer.begin(), cGatedSeqIndex));
			std::copy(std::next(lockedProgramBufferDecoded.begin(), cLayerB + cGatedSeqIndex),
				std::next(lockedProgramBufferDecoded.begin(), cLayerB + cGatedSeqIndex + 4 * 16),
				std::next(programEditBuffer.begin(), cLayerB + cGatedSeqIndex));

			// For the gated to work as expected, take over the switch as well which of the sequencers is on (poly or gated),
			// and we need the gated destination for track 1 to be osc all frequencies
			programEditBuffer[cGatedSeqOnIndex] = lockedProgramBufferDecoded[cGatedSeqOnIndex];
			programEditBuffer[cGatedSeqDestination] = lockedProgramBufferDecoded[cGatedSeqDestination];
			programEditBuffer[cLayerB + cGatedSeqOnIndex] = lockedProgramBufferDecoded[cLayerB + cGatedSeqOnIndex];
			programEditBuffer[cLayerB + cGatedSeqDestination] = lockedProgramBufferDecoded[cLayerB + cGatedSeqDestination];

			// Also copy over tempo and clock
			programEditBuffer[cBpmTempo] = lockedProgramBufferDecoded[cBpmTempo];
			programEditBuffer[cClockDivide] = lockedProgramBufferDecoded[cClockDivide];
			programEditBuffer[cLayerB + cBpmTempo] = lockedProgramBufferDecoded[cLayerB + cBpmTempo];
			programEditBuffer[cLayerB + cClockDivide] = lockedProgramBufferDecoded[cLayerB + cClockDivide];
		});
	}

	NrpnDefinition Rev2::nrpn(std::string const &name)
	{
		for (auto n : nrpns) {
			if (n.name() == name) {
				return n;
			}
		}
		throw new std::runtime_error("Unknown nrpn");
	}

	NrpnDefinition Rev2::nrpn(int nrpnNumber)
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

	std::string Rev2::nameOfNrpn(Rev2Message const &message)
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

	int Rev2::valueOfNrpnInPatch(Rev2Message const &message, PatchData const &patch) {
		// This works when our nrpn definition has a sysex index specified
		for (auto n : nrpns) {
			if (n.matchesController(message.nrpnController())) {
				return valueOfNrpnInPatch(n, patch);
			}
		}
		return 0;
	}

	int Rev2::valueOfNrpnInPatch(NrpnDefinition const &nrpn, PatchData const &patch) {
		int index = nrpn.sysexIndex();
		if (index != -1) {
			if (index >= 0 && index < patch.size()) {
				return patch.at(nrpn.sysexIndex());
			}
		}
		// Invalid value for patch
		return 0;
	}

	void Rev2::switchToLayer(int layerNo)
	{
		// The Rev2 has only two layers, A and B
		// Which of the layers is played is not part of the patch data, but is a global setting/parameter. Luckily, this can be switched via an NRPN message
		// The DSI synths like MSB before LSB
		auto messages = MidiHelpers::generateRPN(channel().toOneBasedInt(), 4190, layerNo, true, true, true);
		MidiController::instance()->getMidiOutput(midiOutput())->sendBlockOfMessagesNow(MidiHelpers::bufferFromMessages(messages));
	}

/*	std::string Rev2::presetName()
	{
		return "Knobkraft Rev2";
	}

	void Rev2::setupBCR2000(MidiController *controller, BCR2000 &bcr, SimpleLogger *logger) {
		if (!bcr.channel().isValid()) return;
		if (!channel().isValid()) return;

		auto bcl = Rev2BCR2000::generateBCR(*this, 10);
		auto syx = bcr.convertToSyx(bcl);
		controller->enableMidiInput(bcr.midiInput()); // Make sure we listen to the answers from the BCR2000 that we detected!
		bcr.sendSysExToBCR(controller->getMidiOutput(bcr.midiOutput()), syx, *controller, logger);
	}

	void Rev2::syncDumpToBCR(MidiProgramNumber programNumber, MidiController *controller, BCR2000 &bcr, SimpleLogger *logger) {
		//TODO not implemented yet, all we could do here is to extract the gated sequencer
	}

	void Rev2::setupBCR2000View(BCR2000_Component &view) {
		//TODO - this needs to be implemented
	}

	void Rev2::setupBCR2000Values(BCR2000_Component &view, Patch *patch)
	{
		// TODO
	}*/

	void Rev2::changeInputChannel(MidiController *controller, MidiChannel newChannel, std::function<void()> onFinished)
	{
		// The Rev2 will change its channel with a nice NRPN message
		// See page 87 of the manual
		// Setting it to 0 would be Omni, so we use one based int
		controller->getMidiOutput(midiOutput())->sendBlockOfMessagesNow(createNRPN(4098, newChannel.toOneBasedInt()));
		setCurrentChannelZeroBased(midiInput(), midiOutput(), newChannel.toZeroBasedInt());
		onFinished();
	}

	void Rev2::setMidiControl(MidiController *controller, bool isOn)
	{
		// See page 87 of the manual
		controller->getMidiOutput(midiOutput())->sendBlockOfMessagesNow(createNRPN(4103, isOn ? 1 : 0));
		localControl_ = isOn;
	}

	void Rev2::changeOutputChannel(MidiController *controller, MidiChannel channel, std::function<void()> onFinished)
	{
		// The Rev2 has no split input and output channel. So MIDI routing is vital in this case.
		changeInputChannel(controller, channel, onFinished);
	}

	void Rev2::setLocalControl(MidiController *controller, bool localControlOn)
	{
		controller->getMidiOutput(midiOutput())->sendBlockOfMessagesNow(createNRPN(4107, localControlOn ? 1 : 0));
		localControl_ = localControlOn;
	}

	std::shared_ptr<Patch> Rev2::patchFromProgramDumpSysex(const MidiMessage& message) const
	{
		return patchFromSysex(message);
	}

	std::vector<juce::MidiMessage> Rev2::patchToProgramDumpSysex(const Patch &patch) const
	{
		return patchToSysex(patch);
	}

	std::string Rev2::getName() const
	{
		if (versionString_.empty())
			return "DSI Prophet Rev2";
		else
			return (boost::format("DSI Prophet Rev2 (%s)") % versionString_).str();
	}

	juce::MidiMessage Rev2::clearPolySequencer(const MidiMessage &programEditBuffer, bool layerA, bool layerB)
	{
		return filterProgramEditBuffer(programEditBuffer, [layerA, layerB](std::vector<uint8> &programEditBuffer) {
			// Just fill all 6 tracks of the Poly Sequencer with note 0x3f and velocity 0
			for (int track = 0; track < 6; track++) {
				for (int step = 0; step < 64; step++) {
					if (layerA) {
						programEditBuffer[cStepSeqNote1Index + track * 128 + step] = cDefaultNote;
						programEditBuffer[cStepSeqVelocity1Index + track * 128 + step] = 0x00;
					}
					if (layerB) {
						programEditBuffer[cLayerB + cStepSeqNote1Index + track * 128 + step] = cDefaultNote;
						programEditBuffer[cLayerB + cStepSeqVelocity1Index + track * 128 + step] = 0x00;
					}
				}
			}
		});
	}

}
