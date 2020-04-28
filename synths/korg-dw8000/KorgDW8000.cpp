/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "KorgDW8000.h"

#include "KorgDW8000Patch.h"
#include "KorgDW8000Parameter.h"
//#include "KorgDW8000_BCR2000.h"

//#include "BCR2000.h"

#include "MidiHelpers.h"
#include "Sysex.h"

#include <iterator>
#include <boost/format.hpp>

namespace midikraft {

	std::string KorgDW8000::getName() const
	{
		return "Korg DW 8000";
	}

	juce::MidiMessage KorgDW8000::deviceDetect(int channel)
	{
		// Build Device ID request message
		return MidiHelpers::sysexMessage({ 0x42 /* Korg */, uint8(0x40 | channel) });
	}

	int KorgDW8000::deviceDetectSleepMS()
	{
		// The Korg is reasonably fast to reply, try 100 ms
		return 100;
	}

	MidiChannel KorgDW8000::channelIfValidDeviceResponse(const MidiMessage &message) {
		// Is this the correct Device ID message?
		auto data = message.getSysExData();
		if (message.isSysEx() && data[0] == 0x42 /* Korg */
			&& (data[1] & 0xf0) == 0x30  /* Device ID */
			&& data[2] == 0x03 /* DW 8000 */) {
			return MidiChannel::fromZeroBase(data[1] & 0x0f);
		}
		return MidiChannel::invalidChannel();
	}

	bool KorgDW8000::needsChannelSpecificDetection()
	{
		return true;
	}

	bool KorgDW8000::isOwnSysex(MidiMessage const &message) const
	{
		auto data = message.getSysExData();
		return message.isSysEx() && data[0] == 0x42 /* Korg */
			&& ((data[1] & 0xf0) == 0x30) /* Format */
			//&& ((data[1] & 0x0f) == midiChannel_) /* correct channel */
			&& data[2] == 0x03; /* DW 8000 */
	}

	int KorgDW8000::numberOfBanks() const
	{
		return 1;
	}

	juce::MidiMessage KorgDW8000::requestEditBufferDump()
	{
		// This is called a "Data Save Request" in the Service Manual (p. 6)
		return MidiHelpers::sysexMessage({ 0x42 /* Korg */, uint8(0x30 | channel().toZeroBasedInt()), 0x03 /* Model ID = DW 8000 */, DATA_SAVE_REQUEST });
	}

	/*
	std::vector<juce::MidiMessage> KorgDW8000::requestPatch(int patchNo)
	{
		std::vector<MidiMessage> result;

		// We need to send a program change
		result.push_back(MidiMessage::programChange(channel().toOneBasedInt(), patchNo));
		// And request the edit buffer
		result.push_back(requestEditBufferDump());
		return result;
	}*/

	int KorgDW8000::numberOfPatches() const
	{
		return 64;
	}

	std::string KorgDW8000::friendlyBankName(MidiBankNumber bankNo) const
	{
		ignoreUnused(bankNo);
		return "Standard Bank";
	}

	juce::MidiMessage KorgDW8000::saveEditBufferToProgram(int programNumber)
	{
		// The Korg DW 8000 has no direct download to program slot message - you can only send it the edit buffer, and then instruct it to 
		// save it into a program you specify
		jassert(programNumber >= 0 && programNumber < 64);
		if (programNumber < 0 || programNumber >= 64) {
			return MidiMessage();
		}
		return MidiHelpers::sysexMessage({ 0x42 /* Korg */, uint8(0x30 | channel().toZeroBasedInt()),
			0x03 /* Model ID = DW 8000 */, WRITE_REQUEST, uint8(programNumber) });
	}

	MidiChannel KorgDW8000::getInputChannel() const
	{
		return channel();
	}

	/*std::vector<std::string> KorgDW8000::presetNames()
	{
		return { (boost::format("Knobkraft %s") % getName()).str() };
	}*/

	bool KorgDW8000::isEditBufferDump(const MidiMessage& message) const
	{
		auto data = message.getSysExData();
		return isOwnSysex(message)
			&& data[3] == DATA_DUMP;
	}

	std::shared_ptr<Patch> KorgDW8000::patchFromSysex(const MidiMessage& message) const
	{
		// The DW8000 is so primitive that it does do nothing to the few bytes of data it needs per patch
		if (!isEditBufferDump(message)) {
			jassert(false);
			return {};
		}

		// Extract the data
		auto data = message.getSysExData();
		Synth::PatchData patchdata;
		for (int index = 4; index < message.getSysExDataSize(); index++) {
			patchdata.push_back(data[index]);
		}
		return {std::make_shared<KorgDW8000Patch>(patchdata, MidiProgramNumber::fromZeroBase(0))};
	}

	std::shared_ptr<DataFile> KorgDW8000::patchFromPatchData(const Synth::PatchData &data, MidiProgramNumber place) const {
		return std::make_shared<KorgDW8000Patch>(data, place);
	}

	std::vector<juce::MidiMessage> KorgDW8000::patchToSysex(const Patch &patch) const
	{
		// For the patch sysex, all we need to do is to add the header on the correct channel
		std::vector<uint8> data(4);
		data[0] = 0x42; // Korg
		data[1] = uint8(0x30 | channel().toZeroBasedInt());
		data[2] = 0x03; // DW8000
		data[3] = DATA_DUMP;
		std::copy(patch.data().begin(), patch.data().end(), std::back_inserter(data));

		return std::vector<MidiMessage>({ MidiHelpers::sysexMessage(data) });
	}

	/*using namespace BinaryData;

	// These are the filenames for the various ROMs from the Internet which contain the real wave forms of the DWGS system
	// http://dbwbp.com/index.php/9-misc/37-synth-eprom-dumps
	std::vector<const char *> kDR8000ROMs = {
		HN613256PT70_bin, HN613256PT71_bin, HN613256PCB4_bin, HN613256PCB5_bin, EXP1_bin, EXP3_bin, EXP2_bin, EXP4_bin
	};

	std::vector<float> KorgDW8000::romWave(int waveNo)
	{
		std::vector<float> result;
		if (waveNo < 0 || waveNo > 32) {
			jassert(false);
			return result;
		}

		// Select the right ROM binary resource
		int romNo = waveNo / 4;
		const uint8 *rom = reinterpret_cast<const uint8 *>(kDR8000ROMs[romNo]);

		// Each ROM contains 4 waves, 8bit unsigned format, with the following data:
		//2048 samples octave 0
		//2048 samples octave 1
		//1024 samples octave 2
		//1024 samples octave 3
		//512 samples octave 4
		//512 samples octave 5
		//512 samples octave 6
		//512 samples octave 7

		int base = (waveNo % 4) * 8192; // 8k per wave, 4 waves per file. The Kawai K3 is much more subtle in its design
		for (int step = 0; step < 2048; step++) {
			result.push_back(rom[base + step]);
		}
		return result;
	}

	std::vector<std::string> waveNames = {
		"Sawtooth", "Square", "Piano", "Electric piano 1", "Electric piano 2", "Clavinet", "Organ", "Brass", "Sax", "Violin", "Guitar", "Electric guitar", "Bass", "Digital bass", "Bell and whistle"
	};

	std::string KorgDW8000::waveName(int waveNo)
	{
		if (waveNo < 0 || waveNo > 15) {
			jassert(false);
			return "unknown";
		}
		return waveNames[waveNo];
	}

	void KorgDW8000::createReverseEngineeringData() {
		std::vector<juce::MidiMessage> reverseEngineer;


		// Ok, 51 parameters to fit into 64 patches... that should be easy.
		for (int patch = 0; patch < 64; patch++) {
			// Create completely empty patch - 51 bytes 0
			std::vector<uint8> data(51);

			if (patch < KorgDW8000Parameter::allParameters.size()) {
				// set this one parameter to its max value
				data[KorgDW8000Parameter::allParameters[patch]->sysexIndex()] = (uint8)KorgDW8000Parameter::allParameters[patch]->maxValue();
			}

			KorgDW8000Patch testPatch(data, (boost::format("Patch %d") % patch).str());
			reverseEngineer.push_back(patchToSysex(testPatch)[0]);
			reverseEngineer.push_back(saveEditBufferToProgram(patch));
		}

		Sysex::saveSysex("reveng.syx", reverseEngineer);
	}

	void KorgDW8000::setupBCR2000(MidiController *controller, BCR2000 &bcr, SimpleLogger *logger) {
		if (!bcr.channel().isValid()) return;
		if (!channel().isValid()) return;

		auto bcl = KorgDW8000BCR2000::generateBCL(channel().toZeroBasedInt());
		auto syx = bcr.convertToSyx(bcl);
		controller->enableMidiInput(bcr.midiInput()); // Make sure we listen to the answers from the BCR2000 that we detected!
		bcr.sendSysExToBCR(controller->getMidiOutput(bcr.midiOutput()), syx, *controller, logger);
	}

	void KorgDW8000::syncDumpToBCR(MidiProgramNumber programNumber, MidiController *controller, BCR2000 &bcr, SimpleLogger *logger) {
	}

	void KorgDW8000::setupBCR2000View(BCR2000_Component &view) {
		//TODO
	}

	void KorgDW8000::setupBCR2000Values(BCR2000_Component &view, Patch *patch)
	{
		//TODO
	}*/

}
