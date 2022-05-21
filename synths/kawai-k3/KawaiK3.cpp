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
		// We cannot write sysex to an invalid or OMNI channel, so in this case use channel 1
		uint8 c = channel().isValid() ? (channel().toZeroBasedInt() & 0x0f) : 0;
		return { 0x40 /* Kawai */, uint8(0x00 | c), (uint8)function, 0x00 /* Group No */, 0x01 /* Kawai K3 */,  subcommand };
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
		// The Kawai K3 seems to be fast, just 40 ms wait time. But if the MIDI network is loaded, e.g. because the MKS80 repeats many messages, 
		// we should rather be a bit cautious
		return 200;
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
			buildSysexFunctionMessage(ALL_BLOCK_DATA_REQUEST, (uint8)bankNo.toZeroBased()),
			requestWaveBufferDump(bankNo.toZeroBased() == 0 ? WaveType::USER_WAVE : WaveType::USER_WAVE_CARTRIDGE)
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

	std::string KawaiK3::friendlyProgramName(MidiProgramNumber programNo) const
	{
		switch (programNo.toZeroBased()) {
		case 100: return "Internal Wave";
		case 101: return "Cartridge Wave";
		default:
			return (boost::format("%02d") % programNo.toOneBased()).str();
		}
	}

	bool KawaiK3::isSingleProgramDump(const std::vector<MidiMessage>& message) const
	{
		if (message.size() == 1 && sysexFunction(message[0]) == ONE_BLOCK_DATA_DUMP) {
			uint8 patchNo = sysexSubcommand(message[0]);
			return patchNo < 100;
		}
		return false;
	}

	MidiProgramNumber KawaiK3::getProgramNumber(const std::vector<MidiMessage>& message) const
	{
		if (isSingleProgramDump(message)) {
			return MidiProgramNumber::fromZeroBase(sysexSubcommand(message[0]));
		}
		return MidiProgramNumber::fromZeroBase(0);
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

		if (data.size() == 34 || data.size() == 98) {
			return std::make_shared<KawaiK3Patch>(place, data);
		}
		else if (data.size() == 64) {
			return std::make_shared<KawaiK3Wave>(data, place);
		}

		// TODO this is migration for data from a buggy version
		if (data.size() == 65) {
			// Migration of old data from version 1.3.0 had an extra byte
			return std::make_shared<KawaiK3Wave>(std::vector<uint8>(data.begin(), data.begin() + 64), place);
		}
		if (data.size() == 99) {
			return std::make_shared<KawaiK3Wave>(std::vector<uint8>(data.begin(), data.begin() + 98), place);
		}

		jassertfalse;
		return {};
	}

	std::shared_ptr<DataFile> KawaiK3::patchFromProgramDumpSysex(const std::vector<MidiMessage>& message) const
	{
		if (isSingleProgramDump(message)) {
			return k3PatchFromSysex(message[0], 0);
		}
		return {};
	}

	std::vector<juce::MidiMessage> KawaiK3::patchToProgramDumpSysex(std::shared_ptr<DataFile> patch, MidiProgramNumber programNumber) const
	{
		return { k3PatchToSysex(patch->data(), programNumber.toZeroBased(), false) };
	}

	std::shared_ptr<Patch> KawaiK3::k3PatchFromSysex(const MidiMessage& message, int indexIntoBankDump /* = 0 */) const {
		// Parse the sysex and build a patch class that allows access to the individual params in that patch
		if (isSingleProgramDump({ message }) || isBankDumpAndNotWaveDump(message)) {
			// Build the patch data ("tone data") from the nibbles
			std::vector<uint8> data;
			auto rawData = message.getSysExData();
			int readIndex = 6 + indexIntoBankDump * 35 * 2;
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
					jassert(toneData.size() == 34);
					if (isSingleProgramDump({ message })) {
						return std::make_shared<KawaiK3Patch>(getProgramNumber({ message }), toneData);
					}
					else {
						return std::make_shared<KawaiK3Patch>(MidiProgramNumber::fromZeroBase(indexIntoBankDump), toneData);
					}
				}
				else {
					SimpleLogger::instance()->postMessage((boost::format("Checksum error when loading Kawai K3 patch. Expected %02X but got %02X") % (int) data[34] % (sum & 0xff)).str());
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
				result.push_back(k3PatchFromSysex(message,i));
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
				for (size_t i = 0; i < 64; i++) {
					sum += data[i];
					waveData.push_back(data[i]);
				}
				if (data[64] == (sum & 0xff)) {
					// CRC check successful
					uint8 waveNo = sysexSubcommand(message);
					jassert(waveData.size() == 64);
					return std::make_shared<KawaiK3Wave>(waveData, MidiProgramNumber::fromZeroBase(waveNo));
				}
				else {
					SimpleLogger::instance()->postMessage((boost::format("Checksum error when loading Kawai K3 wave. Expected %02X but got %02X") % (int) data[64] % (sum & 0xff)).str());
				}
			}
		}
		return {};
	}

	juce::MidiMessage KawaiK3::waveToSysex(std::shared_ptr<KawaiK3Wave> wave)
	{
		return k3PatchToSysex(wave->data(), static_cast<int>(KawaiK3::WaveType::USER_WAVE), false);
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
				if (currentWave) {
					result.push_back(currentWave);

					// And if we have unresolved patches, add this wave to them (the convention seems to be patches first, at the very end the user wave)
					// Though I only found one factory bank on the Kawai US website that has the user wave stored (K3GINT.SYX)
					for (auto patch : unresolvedUserWave) {
						patch->addWaveIfOscillatorUsesIt(currentWave);
					}
					unresolvedUserWave.clear();
				}
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
				}
			}
			else if (isSingleProgramDump({ message })) {
				auto newPatch = std::dynamic_pointer_cast<KawaiK3Patch>(patchFromProgramDumpSysex({ message }));
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
			return isSingleProgramDump({ message });
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
					result.push_back(patchFromProgramDumpSysex({ message }));
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

	std::vector<juce::MidiMessage> KawaiK3::dataFileToMessages(std::shared_ptr<DataFile> dataFile, std::shared_ptr<SendTarget> target) const
	{
		ignoreUnused(target);
		std::vector<juce::MidiMessage> result;
		switch (dataFile->dataTypeID())
		{
		case K3_PATCH:
			return { k3PatchToSysex(dataFile->data(), kFakeEditBuffer.toZeroBased(), false), k3PatchToSysex(dataFile->data(), static_cast<int>(WaveType::USER_WAVE), true) };
		case K3_WAVE:
			return { k3PatchToSysex(dataFile->data(), 100, false) };
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

	midikraft::Synth::PatchData KawaiK3::createInitPatch()
	{
		return KawaiK3Patch::createInitPatch()->data();
	}

	MidiChannel KawaiK3::getInputChannel() const
	{	
		return channel();
	}

	juce::MidiMessage KawaiK3::k3PatchToSysex(Synth::PatchData const &patch, int programNo, bool produceWaveInsteadOfPatch) const
	{
		if (programNo < 0 || programNo > 101) {
			jassert(false);
			return MidiMessage();
		}

		size_t start = 0;
		size_t end = programNo < 100 ? 34 : 64;
		if (produceWaveInsteadOfPatch && patch.size() == 98) {
			start = 34;
			end = 34 + 64;
		}
		if (patch.size() >= end) {
			// This is just the reverse nibbling for that patch data...
			// Works for the wave as well
			std::vector<uint8> data = buildSysexFunction(ONE_BLOCK_DATA_DUMP, (uint8)programNo);
			uint8 sum = 0;
			for (size_t i = start; i < end; i++) {
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

	void KawaiK3::sendDataFileToSynth(std::shared_ptr<DataFile> dataFile, std::shared_ptr<SendTarget> target) {
		ignoreUnused(target);
		auto wave = std::dynamic_pointer_cast<KawaiK3Wave>(dataFile);
		if (wave) {
			SimpleLogger::instance()->postMessage("Writing K3 user wave to the internal wave memory");
		}
		else {
			SimpleLogger::instance()->postMessage((boost::format("Writing K3 patch '%s' to program %s") % dataFile->name() % friendlyProgramName(kFakeEditBuffer)).str());
		}
		MidiBuffer messages = MidiHelpers::bufferFromMessages(dataFileToMessages(dataFile, target));
		sendPatchToSynth(MidiController::instance(), SimpleLogger::instance(), messages);
	}

	void KawaiK3::sendPatchToSynth(MidiController * controller, SimpleLogger * logger, MidiBuffer const &messages) {
		auto secondHandler = MidiController::makeOneHandle();
		MidiController::instance()->addMessageHandler(secondHandler, [this, logger, controller, secondHandler](MidiInput *source, MidiMessage const &message) {
			ignoreUnused(source);
			if (isWriteConfirmation(message)) {
				MidiController::instance()->removeMessageHandler(secondHandler);
				logger->postMessage("Got patch write confirmation from K3");
				MidiBuffer midiBuffer;
                midiBuffer.addEvent(MidiMessage::programChange(channel().toOneBasedInt(), 1), 1); // Any program can be used
                midiBuffer.addEvent(MidiMessage::programChange(channel().toOneBasedInt(), kFakeEditBuffer.toZeroBased()), 2);
				controller->getMidiOutput(midiOutput())->sendBlockOfMessagesFullSpeed(midiBuffer);
				// We ignore the result of these sends, just hope for the best
			}
			else {
				//TODO the message handler is never removed in case you do not get a write confirmation. This will crash.
			}
		});
		controller->enableMidiOutput(midiOutput());
		controller->enableMidiInput(midiInput());
		controller->getMidiOutput(midiOutput())->sendBlockOfMessagesFullSpeed(messages);
	}

	void KawaiK3::sendBlockOfMessagesToSynth(std::string const& midiOutput, std::vector<MidiMessage> const& buffer)
	{
		// We need to inspect if in there are any patch dumps are wave dump messages
		auto midiOut = MidiController::instance()->getMidiOutput(midiOutput);
		std::vector<MidiMessage> filtered;
		std::shared_ptr<MidiMessage> patchToSend;
		std::shared_ptr<MidiMessage> waveToSend;
		for (const auto& message : buffer) {
			// Suppress empty sysex messages, they seem to confuse vintage hardware (the Kawai K3 in particular)
			if (MidiHelpers::isEmptySysex(message)) continue;

			// Special handling required for patch dumps and wave dumps!
			if (isSingleProgramDump({ message })) {
				patchToSend = std::make_shared<MidiMessage>(message);
			}
			else if (isWaveBufferDump(message)) {
				waveToSend = std::make_shared<MidiMessage>(message);
			}
			else {
				filtered.push_back(message);
			}
		}
		// Send the filtered stuff
		midiOut->sendBlockOfMessagesFullSpeed(MidiHelpers::bufferFromMessages(filtered));
		if (patchToSend) {
			sendPatchToSynth(MidiController::instance(), SimpleLogger::instance(), MidiHelpers::bufferFromMessages({ *patchToSend }));
		}
		if (waveToSend) {
			sendPatchToSynth(MidiController::instance(), SimpleLogger::instance(), MidiHelpers::bufferFromMessages({ *waveToSend}));
		}
	}

	void KawaiK3::setupBCR2000(BCR2000& bcr) {
		if (!bcr.wasDetected()) return;
		if (!channel().isValid()) return;

		// Use MIDI channel 16 to not interfere with any other MIDI hardware accidentally taking the CC messages for real
		std::string presetName = (boost::format("Knobkraft %s %d") % getName() % channel().toOneBasedInt()).str();
		auto bcl = KawaiK3BCR2000::generateBCL(presetName, bcr.channel(), channel());
		auto syx = bcr.convertToSyx(bcl);
		MidiController::instance()->enableMidiInput(bcr.midiInput()); // Make sure we listen to the answers from the BCR2000 that we detected!
		bcr.sendSysExToBCR(MidiController::instance()->getMidiOutput(bcr.midiOutput()), syx, [](std::vector<BCR2000::BCRError> errors) {
			//TODO
			ignoreUnused(errors);
		});
	}

	void KawaiK3::setupBCR2000View(BCR2000Proxy* view, TypedNamedValueSet& parameterModel, ValueTree& valueTree)
	{
		// This needs the specific controller layout for putting the 39 parameters onto the 32 knobs of the BCR2000
		KawaiK3BCR2000::setupBCR2000View(view, parameterModel, valueTree);
	}

}
