/*
   Copyright (c) 2019 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "Rev2.h"

#include "Patch.h"

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
