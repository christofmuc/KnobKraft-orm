/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "KawaiK3.h"

#include "KawaiK3Parameter.h"
#include "KawaiK3Patch.h"
#include "KawaiK3Wave.h"
#include "KawaiK3_BCR2000.h"

#include "Sysex.h"

#include "Logger.h"
#include "MidiController.h"

#include "MidiHelpers.h"
#include "MidiProgramNumber.h"

#include "BinaryResources.h"

#include <boost/format.hpp>

namespace midikraft {

	// The K3 does not sport a proper edit buffer, which is why you need to sacrifice one slot of the 50 for volatile patches
	const MidiProgramNumber KawaiK3::kFakeEditBuffer = MidiProgramNumber::fromOneBase(50);

	const std::set<KawaiK3::SysexFunction> KawaiK3::validSysexFunctions = {
		ONE_BLOCK_DATA_REQUEST,
		ALL_BLOCK_DATA_REQUEST,
		PARAMETER_SEND,
		ONE_BLOCK_DATA_DUMP,
		ALL_BLOCK_DATA_DUMP,
		WRITE_COMPLETE,
		WRITE_ERROR,
		WRITE_ERROR_BY_NO_CARTRIDGE,
		WRITE_ERROR_BY_PROTECT,
		MACHINE_ID_REQUEST,
		MACHINE_ID_ACKNOWLEDE
	};

	KawaiK3::KawaiK3() : programNo_(kFakeEditBuffer)
	{
	}

	std::string KawaiK3::getName() const
	{
		return "Kawai K3/K3M";
	}

	std::vector<juce::MidiMessage> KawaiK3::deviceDetect(int channel)
	{
		// Build Device ID request message. Manual p. 48. Why is this shorter than all other command messages?
		std::vector<uint8> sysEx({ 0x40 /* Kawai */, uint8(0x00 | channel), MACHINE_ID_REQUEST });
		return { MidiHelpers::sysexMessage(sysEx) };
	}

	std::vector<uint8> KawaiK3::buildSysexFunction(SysexFunction function, uint8 subcommand) const {
		return { 0x40 /* Korg */, uint8(0x00 | channel().toZeroBasedInt()), (uint8)function, 0x00 /* Group No */, 0x01 /* Kawai K3 */,  subcommand };
	}

	juce::MidiMessage KawaiK3::buildSysexFunctionMessage(SysexFunction function, uint8 subcommand) const {
		return MidiHelpers::sysexMessage(buildSysexFunction(function, subcommand));
	}

	KawaiK3::SysexFunction KawaiK3::sysexFunction(juce::MidiMessage const &message) const
	{
		if (isOwnSysex(message) && message.getSysExDataSize() > 2) {
			SysexFunction function = static_cast<SysexFunction>(message.getSysExData()[2]);
			if (validSysexFunctions.find(function) != validSysexFunctions.end()) {
				return function;
			}
		}
		return INVALID_FUNCTION;
	}

	uint8 KawaiK3::sysexSubcommand(juce::MidiMessage const &message) const
	{
		if (isOwnSysex(message) && message.getSysExDataSize() > 5) {
			return message.getSysExData()[5];
		}
		jassertfalse;
		return 255;
	}

	int KawaiK3::deviceDetectSleepMS()
	{
		// The Kawai K3 seems to be fast, just 40 ms wait time.
		return 40;
	}

	MidiChannel KawaiK3::channelIfValidDeviceResponse(const MidiMessage &message)
	{
		// Is this the correct Device ID message?
		if (message.isSysEx()
			&& message.getSysExDataSize() > 2
			&& message.getSysExData()[0] == 0x40 // Kawai 
			&& (message.getSysExData()[1] & 0xf0) == 0x00  // Device ID 
			&& message.getSysExData()[2] == MACHINE_ID_ACKNOWLEDE) {
			return MidiChannel::fromZeroBase(message.getSysExData()[1] & 0x0f);
		}
		return MidiChannel::invalidChannel();
	}

	bool KawaiK3::needsChannelSpecificDetection()
	{
		return true;
	}

	bool KawaiK3::isOwnSysex(MidiMessage const &message) const
	{
		if (message.isSysEx()) {
			auto data = message.getSysExData();
			return (data[0] == 0x40 /* Kawai */
				&& (data[1] & 0xf0) == 0x00  /* 0 except channel */
				&& data[3] == 0 /* Group No */
				&& data[4] == 1 /* K3 ID */
				);
		}
		return false;
	}

	int KawaiK3::numberOfBanks() const
	{
		return 2; // 2 only with an inserted RAM cartridge. How can we check that it is inserted?
	}

	std::vector<MidiMessage> KawaiK3::requestBankDump(MidiBankNumber bankNo) const
	{
		// When we request a bank dump from the K3, we always also request the user wave, because if one of the patches
		// references the wave, we need to know which wave?
		return {
			requestWaveBufferDump(bankNo.toZeroBased() == 0 ? WaveType::USER_WAVE : WaveType::USER_WAVE_CARTRIDGE),
			buildSysexFunctionMessage(ALL_BLOCK_DATA_REQUEST, (uint8)bankNo.toZeroBased())
		};
	}

	std::vector<juce::MidiMessage> KawaiK3::requestPatch(int patchNo) const
	{
		// This is called a "One Block Data Request" in the Manual (p. 48)
		if (patchNo < 0 || patchNo > 101) {
			jassertfalse;
			return {};
		}
		return {buildSysexFunctionMessage(ONE_BLOCK_DATA_REQUEST, (uint8)patchNo)};
	}

	int KawaiK3::numberOfPatches() const
	{
		return 50;
	}

	bool KawaiK3::isSingleProgramDump(const MidiMessage& message) const
	{
		if (sysexFunction(message) == ONE_BLOCK_DATA_DUMP) {
			uint8 patchNo = sysexSubcommand(message);
			return patchNo < 100;
		}
		return false;
	}

	juce::MidiMessage KawaiK3::requestWaveBufferDump(WaveType waveType) const
	{
		return requestPatch(static_cast<int>(waveType))[0];
	}

	bool KawaiK3::isWaveBufferDump(const MidiMessage& message) const
	{
		if (sysexFunction(message) == ONE_BLOCK_DATA_DUMP) {
			uint8 patchNo = sysexSubcommand(message);
			return patchNo >= 100 && patchNo <= 101; // 100 and 101 are the Wave Dumps, 102 is the "MIDI Wave", which doesn't seem to exist in reality, just in the manual
		}
		return false;
	}

	bool KawaiK3::isBankDumpAndNotWaveDump(const MidiMessage& message) const {
		if (sysexFunction(message) == ALL_BLOCK_DATA_DUMP) {
			uint8 bank = sysexSubcommand(message);
			return bank == 0 || bank == 1; // 0 and 1 are the two banks, 1 being the RAM cartridge
		}
		return false;
	}

	bool KawaiK3::isBankDump(const MidiMessage& message) const
	{
		// This should return for all messages that are part of a bank dump, and I want to include the wave dump here as well
		return isBankDumpAndNotWaveDump(message) || isWaveBufferDump(message);
	}

	bool KawaiK3::isBankDumpFinished(std::vector<MidiMessage> const &bankDump) const
	{
		// For the K3, the bank is a single Midi Message. But we have requested also the WaveDump, so we need to check if both are present in the
		// stream
		bool hasBank = false;
		bool hasWave = false;
		for (const auto& message : bankDump) {
			if (isBankDumpAndNotWaveDump(message)) hasBank = true;
			if (isWaveBufferDump(message)) hasWave = true;
		}
		return hasBank && hasWave;
	}

	std::string KawaiK3::friendlyBankName(MidiBankNumber bankNo) const
	{
		switch (bankNo.toZeroBased()) {
		case 0: return "Internal Bank";
		case 1: return "Cartridge";
		default: return "Invalid bank number";
		}
	}

	std::shared_ptr<DataFile> KawaiK3::patchFromPatchData(const Synth::PatchData &data, MidiProgramNumber place) const {
		if (data.size() == 34 || data.size() == 99) {
			return std::make_shared<KawaiK3Patch>(place, data);
		}
		else if (data.size() == 65) {
			return std::make_shared<KawaiK3Wave>(data, place);
		}
		return {};
	}

	std::shared_ptr<Patch> KawaiK3::patchFromProgramDumpSysex(const MidiMessage& message) const
	{
		if (isSingleProgramDump(message)) {
			auto singlePatch = patchFromSysex(message, MidiProgramNumber::fromZeroBase(0));
			if (singlePatch) {
				singlePatch->setPatchNumber(MidiProgramNumber::fromZeroBase(sysexSubcommand(message)));
				return singlePatch;
			}
		}
		return {};
	}

	std::vector<juce::MidiMessage> KawaiK3::patchToProgramDumpSysex(const Patch &patch) const
	{
		return { patchToSysex(patch.data(), patch.patchNumber()->midiProgramNumber().toZeroBased(), false) };
	}

	std::shared_ptr<Patch> KawaiK3::patchFromSysex(const MidiMessage& message, MidiProgramNumber programIndex) const {
		// Parse the sysex and build a patch class that allows access to the individual params in that patch
		if (isSingleProgramDump(message) || isBankDumpAndNotWaveDump(message)) {
			// Build the patch data ("tone data") from the nibbles
			std::vector<uint8> data;
			auto rawData = message.getSysExData();
			int readIndex = 6 + programIndex.toZeroBased() * 35 * 2;
			int readBytes = 0;
			while (readIndex < message.getSysExDataSize() - 1 && readBytes < 35) {
				uint8 highNibble = rawData[readIndex];
				uint8 lowNibble = rawData[readIndex + 1];
				uint8 value = ((highNibble & 0x0f) << 4) | (lowNibble & 0x0f);
				data.push_back(value);
				readIndex += 2;
				readBytes++;
			}

			// Now check the length and the CRC
			if (data.size() == 35) {
				int sum = 0;
				std::vector<uint8> toneData;
				for (int i = 0; i < 34; i++) {
					sum += data[i];
					toneData.push_back(data[i]);
				}
				if (data[34] == (sum & 0xff)) {
					// CRC check successful
					return std::make_shared<KawaiK3Patch>(programIndex, toneData);
				}
				else {
					SimpleLogger::instance()->postMessage((boost::format("Checksum error when loading Kawai K3 patch. Expected %02X but got %02X") % data[34] % (sum & 0xff)).str());
					//messages.push_back(patchToProgramDumpSysex(KawaiK3Patch(programIndex, toneData))[0]);
				}
			}
			else {
				SimpleLogger::instance()->postMessage((boost::format("Invalid length of data while loading Kawai K3 patch. Expected 35 bytes but got %02X") % data.size()).str());
			}
		}
		else {
			SimpleLogger::instance()->postMessage("MIDI message is neither single program dump nor bank dump from Kawai K3, ignoring data!");
		}
		return {};
	}

	TPatchVector KawaiK3::patchesFromSysexBank(const MidiMessage& message) const
	{
		TPatchVector result;
		if (isBankDumpAndNotWaveDump(message)) {
			// A bank has 50 programs...
			for (int i = 0; i < 50; i++) {
				// This is not really efficient, but for now I'd be good with this
				result.push_back(patchFromSysex(message, MidiProgramNumber::fromZeroBase(i)));
			}
		}
		else {
			jassertfalse;
		}
		return result;
	}

	std::shared_ptr<KawaiK3Wave> KawaiK3::waveFromSysex(const MidiMessage& message) const {
		// Parse the sysex and build a patch class that allows access to the individual params in that patch
		if (isWaveBufferDump(message)) {
			// Build the wave data from the nibbles
			std::vector<uint8> data;
			auto rawData = message.getSysExData();
			int readIndex = 6;
			int readBytes = 0;
			while (readIndex < message.getSysExDataSize() - 1 && readBytes < 65) {
				uint8 highNibble = rawData[readIndex];
				uint8 lowNibble = rawData[readIndex + 1];
				uint8 value = ((highNibble & 0x0f) << 4) | (lowNibble & 0x0f);
				data.push_back(value);
				readIndex += 2;
				readBytes++;
			}

			// Now check the length and the CRC
			if (data.size() == 65) {
				int sum = 0;
				WaveData waveData;
				for (int i = 0; i < 64; i++) {
					sum += data[i];
					waveData.push_back(data[i]);
				}
				if (data[64] == (sum & 0xff)) {
					// CRC check successful
					uint8 waveNo = sysexSubcommand(message);
					return std::make_shared<KawaiK3Wave>(data, MidiProgramNumber::fromZeroBase(waveNo));
				}
				else {
					SimpleLogger::instance()->postMessage((boost::format("Checksum error when loading Kawai K3 wave. Expected %02X but got %02X") % data[64] % (sum & 0xff)).str());
				}
			}
		}
		return {};
	}

	juce::MidiMessage KawaiK3::waveToSysex(std::shared_ptr<KawaiK3Wave> wave)
	{
		return patchToSysex(wave->data(), static_cast<int>(KawaiK3::WaveType::USER_WAVE), false);
	}

	std::vector<float> KawaiK3::romWave(int waveNo)
	{
		std::vector<float> result;
		if (waveNo <= 0 || waveNo > 31) {
			// This could happen if you go in here with the user wave, noise, or the wave turned off
			return result;
		}

		// Load the ROM
		const uint8 *romData = reinterpret_cast<const uint8 *>(R6P_09_27c256_BIN);
		jassert(R6P_09_27c256_BIN_size == 32768);

		// Build up the address bits!
		// see https://acreil.wordpress.com/2018/07/15/kawai-k3-and-k3m-1986/
		int wa10wa15 = (waveNo & 0x3f) << 10; // The highest bit would select the RAM waveform
		for (int step = 0; step < 32; step++) {
			int wa5wa9 = step << 5;
			for (int m = 0; m < 16; m++) {
				int wa0wa4 = m << 1; // The lowest bit seems to be the one selecting the "multi-sample". But what exactly is that? A second sample?
				result.push_back(romData[wa0wa4 | wa5wa9 | wa10wa15]);
			}
		}
		return result;
	}

	std::string KawaiK3::waveName(int waveNo)
	{
		return KawaiK3Parameter::waveName(waveNo);
	}

	midikraft::TPatchVector KawaiK3::loadSysex(std::vector<MidiMessage> const &sysexMessages)
	{
		midikraft::TPatchVector result;
		std::vector<std::shared_ptr<KawaiK3Patch>> unresolvedUserWave;
		for (auto const &message : sysexMessages) {
			if (isWaveBufferDump(message)) {
				// A new wave, store it itself in the result vector
				auto currentWave = waveFromSysex(message);
				result.push_back(currentWave);

				// And if we have unresolved patches, add this wave to them (the convention seems to be patches first, at the very end the user wave)
				// Though I only found one factory bank on the Kawai US website that has the user wave stored (K3GINT.SYX)
				for (auto patch : unresolvedUserWave) {
					patch->addWaveIfOscillatorUsesIt(currentWave);
				}
				unresolvedUserWave.clear();
			}
			else if (isBankDumpAndNotWaveDump(message)) {
				auto newPatches = patchesFromSysexBank(message);
				for (auto n : newPatches) {
					auto newPatch = std::dynamic_pointer_cast<KawaiK3Patch>(n);
					if (newPatch) {				
						result.push_back(newPatch);
						if (newPatch->needsUserWave()) {
							unresolvedUserWave.push_back(newPatch);
						}
					}
					else {
						jassertfalse;
					}
				}
			}
			else if (isSingleProgramDump(message)) {
				auto newPatch = std::dynamic_pointer_cast<KawaiK3Patch>(patchFromProgramDumpSysex(message));
				if (newPatch) {
					result.push_back(newPatch);
					if (newPatch->needsUserWave()) {
						unresolvedUserWave.push_back(newPatch);
					}
				}
				else {
					jassertfalse;
				}
			}
		}
		if (!unresolvedUserWave.empty()) {
			for (auto patch : unresolvedUserWave) {
				SimpleLogger::instance()->postMessage((boost::format("No user wave recorded for programmable oscillator of patch '%s', sound can not be reproduced") % patch->name()).str());
			}
		}
		return result;
	}

	bool KawaiK3::isWriteConfirmation(MidiMessage const &message)
	{
		return sysexFunction(message) == WRITE_COMPLETE;
	}

	std::vector<juce::MidiMessage> KawaiK3::requestDataItem(int itemNo, int dataTypeID)
	{
		switch (dataTypeID) {
		case K3_PATCH:
			return requestPatch(itemNo);
		case K3_WAVE:
			return { requestWaveBufferDump(itemNo == 0 ? WaveType::USER_WAVE : WaveType::USER_WAVE_CARTRIDGE) };
		default:
			jassertfalse;
			return {};
		}
	}

	int KawaiK3::numberOfDataItemsPerType(int dataTypeID) const
	{
		switch (dataTypeID) {
		case K3_PATCH:
			return numberOfBanks() * numberOfPatches();
		case K3_WAVE:
			return 2;
		default:
			jassertfalse;
			return 0;
		}
	}

	bool KawaiK3::isDataFile(const MidiMessage &message, int dataTypeID) const
	{
		switch (dataTypeID) {
		case K3_PATCH:
			return isSingleProgramDump(message);
		case K3_WAVE:
			return isWaveBufferDump(message);
		default:
			return false;
		}
	}

	std::vector<std::shared_ptr<midikraft::DataFile>> KawaiK3::loadData(std::vector<MidiMessage> messages, int dataTypeID) const
	{
		std::vector<std::shared_ptr<midikraft::DataFile>> result;
		for (auto const &message : messages) {
			if (isDataFile(message, dataTypeID)) {
				switch (dataTypeID)
				{
				case K3_PATCH:
					result.push_back(patchFromProgramDumpSysex(message));
					break;
				case K3_WAVE:
					result.push_back(waveFromSysex(message));
					break;
				default:
					jassertfalse;
					break;
				}
			}
		}
		return result;
	}

	std::vector<midikraft::DataFileLoadCapability::DataFileDescription> KawaiK3::dataTypeNames() const
	{
		return { { "Patch", true, true}, { "User Wave", true, true} };
	}

	std::vector<juce::MidiMessage> KawaiK3::dataFileToMessages(std::shared_ptr<DataFile> dataFile) const
	{
		std::vector<juce::MidiMessage> result;
		switch (dataFile->dataTypeID())
		{
		case K3_PATCH:
			return { patchToSysex(dataFile->data(), kFakeEditBuffer.toZeroBased(), false), patchToSysex(dataFile->data(), static_cast<int>(WaveType::USER_WAVE), true) };
		case K3_WAVE:
			return { patchToSysex(dataFile->data(), 100, false) };
		default:
			break;
		}
		return result;
	}

	std::vector<std::shared_ptr<midikraft::SynthParameterDefinition>> KawaiK3::allParameterDefinitions() const
	{
		Synth::PatchData data;
		KawaiK3Patch fake(MidiProgramNumber::fromZeroBase(0), data);
		return fake.allParameterDefinitions();
	}

	bool KawaiK3::determineParameterChangeFromSysex(std::vector<juce::MidiMessage> const& messages, std::shared_ptr<SynthParameterDefinition>* outParam, int& outValue)
	{
		for (auto message : messages) {
			if (isOwnSysex(message)) {
				if (sysexFunction(message) == KawaiK3::PARAMETER_SEND) {
					// Yep, that's us. Find the parameter definition and calculate the new value of that parameter
					auto paramNo = sysexSubcommand(message);
					if (paramNo >= 1 && paramNo <= 39) {
						auto paramFound = KawaiK3Parameter::findParameter(static_cast<KawaiK3Parameter::Parameter>(paramNo));
						if (paramFound) {
							if (message.getSysExDataSize() > 7) {
								uint8 highNibble = message.getSysExData()[6];
								uint8 lowNibble = message.getSysExData()[7];
								int value = (highNibble << 4) | lowNibble;
								if (paramFound->minValue() < 0) {
									// Special handling for sign bit in K3
									if ((value & 0x80) == 0x80) {
										value = -(value & 0x7f);
									}
								}

								// Only now we do set our output variables
								*outParam = paramFound;
								outValue = value;
								return true;
							}
						}
					}
				}
			}
		}
		return false;
	}

	void KawaiK3::gotProgramChange(MidiProgramNumber newNumber)
	{
		programNo_ = newNumber;
	}

	MidiProgramNumber KawaiK3::lastProgramChange() const
	{
		return programNo_;
	}

	std::vector<std::string> KawaiK3::presetNames()
	{
		return { (boost::format("Knobkraft %s %d") % getName() % channel().toOneBasedInt()).str() };
	}


	MidiChannel KawaiK3::getInputChannel() const
	{
		return channel();
	}

	juce::MidiMessage KawaiK3::patchToSysex(Synth::PatchData const &patch, int programNo, bool produceWaveInsteadOfPatch) const
	{
		if (programNo < 0 || programNo > 101) {
			jassert(false);
			return MidiMessage();
		}

		int start = 0;
		int end = programNo < 100 ? 34 : 65;
		if (produceWaveInsteadOfPatch) {
			start = 34;
			end = 34 + 64;
		}
		if (patch.size() >= end) {
			// This is just the reverse nibbling for that patch data...
			// Works for the wave as well
			std::vector<uint8> data = buildSysexFunction(ONE_BLOCK_DATA_DUMP, (uint8)programNo);
			uint8 sum = 0;
			for (int i = start; i < end; i++) {
				auto byte = patch.at(i);
				sum += byte;
				data.push_back((byte & 0xf0) >> 4);
				data.push_back(byte & 0x0f);
			}
			data.push_back((sum & 0xf0) >> 4);
			data.push_back(sum & 0x0f);
			return MidiMessage::createSysExMessage(&data[0], static_cast<int>(data.size()));
		}
		else {
			return MidiMessage();
		}
	}

	void KawaiK3::selectRegistration(Patch *currentPatch, DrawbarOrgan::RegistrationDefinition selectedRegistration)
	{
		selectHarmonics(currentPatch, selectedRegistration.first, selectedRegistration.second);
	}

	void KawaiK3::selectHarmonics(Patch *currentPatch, std::string const &name, Additive::Harmonics const &selectedHarmonics)
	{
		auto samples = Additive::createSamplesFromHarmonics(selectedHarmonics);
		//wave1_.displaySampledWave(samples);
		auto wave = std::make_shared<KawaiK3Wave>(selectedHarmonics, MidiProgramNumber::fromZeroBase(static_cast<int>(WaveType::USER_WAVE)));
		auto userWave = waveToSysex(wave);
		SimpleLogger::instance()->postMessage((boost::format("Sending user wave for registration %s to K3") % name).str());
		ignoreUnused(currentPatch);
		//sendK3Wave(MidiController::instance(), MidiController::instance(), SimpleLogger::instance(), currentPatch, userWave);
	}

	void KawaiK3::sendPatchToSynth(MidiController *controller, SimpleLogger *logger, std::shared_ptr<DataFile> dataFile) {
		MidiBuffer messages = MidiHelpers::bufferFromMessages(dataFileToMessages(dataFile));
		auto secondHandler = MidiController::makeOneHandle();
		MidiController::instance()->addMessageHandler(secondHandler, [this, logger, controller, secondHandler](MidiInput *source, MidiMessage const &message) {
			ignoreUnused(source);
			if (isWriteConfirmation(message)) {
				MidiController::instance()->removeMessageHandler(secondHandler);
				logger->postMessage("Got patch write confirmation from K3");
				MidiBuffer messages;
				messages.addEvent(MidiMessage::programChange(channel().toOneBasedInt(), 1), 1); // Any program can be used
				messages.addEvent(MidiMessage::programChange(channel().toOneBasedInt(), kFakeEditBuffer.toZeroBased()), 2);
				controller->getMidiOutput(midiOutput())->sendBlockOfMessagesNow(messages);
				// We ignore the result of these sends, just hope for the best
			}
		});
		auto wave = std::dynamic_pointer_cast<KawaiK3Wave>(dataFile);
		if (wave) {
			logger->postMessage("Writing K3 user wave to the internal wave memory");
		}
		else {
			logger->postMessage((boost::format("Writing K3 patch '%s' to program %s") % dataFile->name() % (KawaiK3PatchNumber(kFakeEditBuffer).friendlyName())).str());
		}
		controller->enableMidiOutput(midiOutput());
		controller->enableMidiInput(midiInput());
		controller->getMidiOutput(midiOutput())->sendBlockOfMessagesNow(messages);
	}


	void KawaiK3::setupBCR2000(BCR2000 &bcr) {
		if (!bcr.channel().isValid()) return;
		if (!channel().isValid()) return;

		// Use MIDI channel 16 to not interfere with any other MIDI hardware accidentally taking the CC messages for real
		auto bcl = KawaiK3BCR2000::generateBCL(presetNames()[0], bcr.channel(), channel());
		auto syx = bcr.convertToSyx(bcl);
		MidiController::instance()->enableMidiInput(bcr.midiInput()); // Make sure we listen to the answers from the BCR2000 that we detected!
		bcr.sendSysExToBCR(MidiController::instance()->getMidiOutput(bcr.midiOutput()), syx, [](std::vector<BCR2000::BCRError> errors) {
			//TODO
			ignoreUnused(errors);
		});
	}

	void KawaiK3::syncDumpToBCR(MidiProgramNumber programNumber, BCR2000 &bcr) {
		if (k3BCRSyncHandler_.isNull()) {
			k3BCRSyncHandler_ = MidiController::makeOneHandle();
			MidiController::instance()->addMessageHandler(k3BCRSyncHandler_, [this, &bcr](MidiInput *source, MidiMessage const &message) {
				ignoreUnused(source);
				if (isSingleProgramDump(message)) {
					SimpleLogger::instance()->postMessage("Got Edit Buffer from K3 and now updating the BCR2000");
					auto currentPatch = std::dynamic_pointer_cast<KawaiK3Patch>(patchFromProgramDumpSysex(message));
					if (currentPatch) {
						// Loop over all parameters in patch and send out the appropriate CCs, if they are within the BCR2000 definition...
						std::vector<MidiMessage> updates;
						/*for (auto paramDef : KawaiK3Parameter::allParameters) {
							SimpleLogger::instance()->postMessage((boost::format("Setting %s [%d] to %d") % paramDef->name() % paramDef->paramNo() % currentPatch->value(*paramDef)).str());
							auto ccMessage = KawaiK3BCR2000::createMessageforParam(paramDef, *currentPatch, MidiChannel::fromOneBase(16));
							updates.push_back(ccMessage);
						}*/
						MidiBuffer updateBuffer = MidiHelpers::bufferFromMessages(updates);
						// Send this to BCR
						if (bcr.channel().isValid() && MidiController::instance()->getMidiOutput(bcr.midiOutput())) {
							MidiController::instance()->getMidiOutput(bcr.midiOutput())->sendBlockOfMessagesNow(updateBuffer);
						}
					}
					else {
						SimpleLogger::instance()->postMessage("Failure reading sysex stream");
					}
				}
			});
		}
		// We want to talk to the Synth now
		MidiController::instance()->enableMidiInput(midiInput());
		MidiBuffer requestBuffer = MidiHelpers::bufferFromMessages(requestPatch(programNumber.toZeroBased()));
		MidiController::instance()->getMidiOutput(midiOutput())->sendBlockOfMessagesNow(requestBuffer);
	}

	void KawaiK3::setupBCR2000View(BCR2000Proxy* view, TypedNamedValueSet& parameterModel, ValueTree& valueTree)
	{
		// This needs the specific controller layout for putting the 39 parameters onto the 32 knobs of the BCR2000
		KawaiK3BCR2000::setupBCR2000View(view, parameterModel, valueTree);
	}

}
