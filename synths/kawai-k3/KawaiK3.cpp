/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "KawaiK3.h"

#include "KawaiK3Parameter.h"
#include "KawaiK3Patch.h"
//#include "KawaiK3_BCR2000.h"

//#include "BCR2000.h"

#include "Sysex.h"

#include "Logger.h"
#include "MidiController.h"
//#include "SynthView.h"
//#include "BCR2000_Component.h"

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

	KawaiK3::KawaiK3() : currentPatch_()
	{
	}

	std::string KawaiK3::getName() const
	{
		return "Kawai K3/K3M";
	}

	juce::MidiMessage KawaiK3::deviceDetect(int channel)
	{
		// Build Device ID request message. Manual p. 48. Why is this shorter than all other command messages?
		std::vector<uint8> sysEx({ 0x40 /* Kawai */, uint8(0x00 | channel), MACHINE_ID_REQUEST });
		return MidiHelpers::sysexMessage(sysEx);
	}

	std::vector<uint8> KawaiK3::buildSysexFunction(SysexFunction function, uint8 subcommand) const {
		std::vector<uint8> sysEx({ 0x40 /* Korg */, uint8(0x00 | channel().toZeroBasedInt()), (uint8)function, 0x00 /* Group No */, 0x01 /* Kawai K3 */,  subcommand });
		return sysEx;
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
		jassert(false);
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

	juce::MidiMessage KawaiK3::createSetParameterMessage(KawaiK3Parameter *param, int paramValue)
	{
		//TODO - this contains the BCR specific offset, that should not be here!
		uint8 highNibble, lowNibble;
		if (param->minValue() < 0) {
			// For parameters with negative values, we have offset the values by -minValue(), so we need to add minValue() again
			int correctedValue = paramValue + param->minValue(); // minValue is negative, so this will subtract actually something resulting in a negative number...
			int clampedValue = std::min(std::max(correctedValue, param->minValue()), param->maxValue());

			// Now, the K3 unfortunately uses a sign bit for the negative values, which makes it completely impossible to be used directly with the BCR2000
			if (clampedValue < 0) {
				highNibble = (((-clampedValue) & 0xFF) | 0x80) >> 4;
				lowNibble = (-clampedValue) & 0x0F;
			}
			else {
				highNibble = (clampedValue & 0xF0) >> 4;
				lowNibble = (clampedValue & 0x0F);
			}
		}
		else {
			// Just clamp to the min max range
			int correctedValue = std::min(std::max(paramValue, param->minValue()), param->maxValue());
			highNibble = (correctedValue & 0xF0) >> 4;
			lowNibble = (correctedValue & 0x0F);
		}

		// Now build the sysex message (p. 48 of the K3 manual)
		auto dataBlock = buildSysexFunction(PARAMETER_SEND, (uint8)param->paramNo());
		dataBlock.push_back(highNibble);
		dataBlock.push_back(lowNibble);
		return MidiMessage::createSysExMessage(&dataBlock[0], static_cast<int>(dataBlock.size()));
	}

	void KawaiK3::determineParameterChangeFromSysex(juce::MidiMessage const &message, KawaiK3Parameter **param, int *paramValue) {
		if (isOwnSysex(message)) {
			if (sysexFunction(message) == PARAMETER_SEND) {
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
							*param = paramFound;
							*paramValue = value;
						}
					}
				}
			}
		}
	}

	juce::MidiMessage KawaiK3::mapCCtoSysex(juce::MidiMessage const &ccMessage)
	{
		// Ok, not too much to do here I hope
		if (ccMessage.isController()) {
			int value = ccMessage.getControllerValue(); // This will give us 0..127 at most. I assume they have been configured properly for the K3
			int controller = ccMessage.getControllerNumber();
			if (controller >= 1 && controller <= 39) {
				// Ok, this is within the proper range of the Kawai K3 controllers
				auto param = KawaiK3Parameter::findParameter((KawaiK3Parameter::Parameter) controller);
				if (param != nullptr) {
					return createSetParameterMessage(param, value);
				}
			}
		}
		return ccMessage;
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
		return { buildSysexFunctionMessage(ALL_BLOCK_DATA_REQUEST, (uint8)bankNo.toZeroBased()) };
	}

	std::vector<juce::MidiMessage> KawaiK3::requestPatch(int patchNo)
	{
		// This is called a "One Block Data Request" in the Manual (p. 48)
		if (patchNo < 0 || patchNo > 101) {
			jassert(false);
			return std::vector<MidiMessage>();
		}
		return std::vector<MidiMessage>{buildSysexFunctionMessage(ONE_BLOCK_DATA_REQUEST, (uint8)patchNo)};
	}

	int KawaiK3::numberOfPatches() const
	{
		return 50;
	}

	bool KawaiK3::isSingleProgramDump(const MidiMessage& message) const
	{
		if (sysexFunction(message) == ONE_BLOCK_DATA_DUMP) {
			uint8 patchNo = sysexSubcommand(message);
			return (patchNo >= 0 && patchNo < 100);
		}
		return false;
	}

	juce::MidiMessage KawaiK3::requestWaveBufferDump(WaveType waveType)
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

	bool KawaiK3::isBankDump(const MidiMessage& message) const
	{
		if (sysexFunction(message) == ALL_BLOCK_DATA_DUMP) {
			uint8 bank = sysexSubcommand(message);
			return bank == 0 || bank == 1; // 0 and 1 are the two banks, 1 being the RAM cartridge
		}
		return false;
	}

	bool KawaiK3::isBankDumpFinished(std::vector<MidiMessage> const &bankDump) const
	{
		// For the K3, the bank is a single Midi Message. Therefore, we just need to find one bank dump in the data and we're good
		for (auto message : bankDump) {
			if (isBankDump(message)) return true;
		}
		return false;
	}

	std::string KawaiK3::friendlyBankName(MidiBankNumber bankNo) const
	{
		switch (bankNo.toZeroBased()) {
		case 0:
			return "Internal Bank";
		case 1:
			return "Cartridge";
		default:
			return "Invalid bank number";
		}
	}

	std::shared_ptr<DataFile> KawaiK3::patchFromPatchData(const Synth::PatchData &data, MidiProgramNumber place) const {
		auto patch = std::make_shared<KawaiK3Patch>(place, data);
		return patch;
	}

	std::shared_ptr<Patch> KawaiK3::patchFromProgramDumpSysex(const MidiMessage& message) const
	{
		if (isSingleProgramDump(message)) {
			auto singlePatch = patchFromSysex(message, MidiProgramNumber::fromZeroBase(0));
			if (singlePatch) {
				singlePatch->setPatchNumber(MidiProgramNumber::fromZeroBase(sysexSubcommand(message)));
			}
			return singlePatch;
		}
		return nullptr;
	}

	std::vector<juce::MidiMessage> KawaiK3::patchToProgramDumpSysex(const Patch &patch) const
	{
		return std::vector<juce::MidiMessage>({ patchToSysex(patch.data(), patch.patchNumber()->midiProgramNumber().toZeroBased()) });
	}

	std::shared_ptr<Patch> KawaiK3::patchFromSysex(const MidiMessage& message, MidiProgramNumber programIndex) const {
		// Parse the sysex and build a patch class that allows access to the individual params in that patch
		if (isSingleProgramDump(message) || isBankDump(message)) {
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
					//TODO - the debug sysex generation should be replaced by a proper log
					std::vector<MidiMessage> messages;
					messages.push_back(message);
					messages.push_back(patchToProgramDumpSysex(KawaiK3Patch(programIndex, toneData))[0]);
					jassert(false);
				}
			}
			else {
				//TODO - need log about program error
				jassert(false);
			}
		}
		else {
			//TODO - need log about program error
			jassert(false);
		}
		return nullptr;
	}

	TPatchVector KawaiK3::patchesFromSysexBank(const MidiMessage& message) const
	{
		TPatchVector result;
		if (isBankDump(message)) {
			// A bank has 50 programs...
			for (int i = 0; i < 50; i++) {
				// This is not really efficient, but for now I'd be good with this
				result.push_back(patchFromSysex(message, MidiProgramNumber::fromZeroBase(i)));
			}
		}
		else {
			//TODO - need log about program error
			jassert(false);
		}
		return result;
	}

	Additive::Harmonics KawaiK3::waveFromSysex(const MidiMessage& message) {
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
					// Build the Harmonics data structure
					Additive::Harmonics harmonics;
					for (int i = 0; i < 64; i += 2) {
						harmonics.push_back(std::make_pair(data[i], data[i + 1] / 31.0f));
						if (data[i] == 0) {
							// It seems a 0 entry stopped the series
							break;
						}
					}
					return harmonics;
				}
				else {
					//TODO - the debug sysex generation should be replaced by a proper log
					std::vector<MidiMessage> messages;
					messages.push_back(message);
					messages.push_back(patchToProgramDumpSysex(KawaiK3Patch(MidiProgramNumber::fromZeroBase(0) // TODO - this is weird, the wave should be position 100 or 101?
						, waveData))[0]);
					jassert(false);
				}
			}
			else {
				//TODO - need log about program error
				jassert(false);
			}
		}
		else {
			//TODO - need log about program error
			jassert(false);
		}
		return Additive::Harmonics();
	}

	juce::MidiMessage KawaiK3::waveToSysex(const Additive::Harmonics& harmonics)
	{
		std::vector<uint8> harmonicArray(64);

		// Fill the harmonic array appropriately
		int writeIndex = 0;
		for (auto harmonic : harmonics) {
			// Ignore zero harmonic definitions - that's the default, and would make the K3 stop looking at the following harmonics
			uint8 harmonicAmp = (uint8)roundToInt(harmonic.second * 31.0);
			if (harmonicAmp > 0) {
				if (harmonic.first >= 1 && harmonic.first <= 128) {
					harmonicArray[writeIndex] = (uint8)harmonic.first;
					harmonicArray[writeIndex + 1] = harmonicAmp;
					writeIndex += 2;
				}
				else {
					//TODO - need log about program error
					jassert(false);
				}
			}
		}

		KawaiK3Patch wavePatch(MidiProgramNumber::fromZeroBase(static_cast<int>(KawaiK3::WaveType::USER_WAVE)), harmonicArray);
		return patchToSysex(wavePatch.data(), static_cast<int>(KawaiK3::WaveType::USER_WAVE)); //TODO the program number is ignored, that's a shame
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
		
	std::string KawaiK3::waveName(WaveType waveType)
	{
		switch (waveType) {
		case WaveType::USER_WAVE: return "User Wave";
		case WaveType::USER_WAVE_CARTRIDGE: return "User Wave Cartridge";
			//case WaveType::MIDI_WAVE: return "MIDI Wave";
		default:
			jassert(false);
			return "";
		}
	}

	std::string KawaiK3::waveName(int waveNo)
	{
		return KawaiK3Parameter::waveName(waveNo);
	}

	bool KawaiK3::isWriteConfirmation(MidiMessage const &message)
	{
		return sysexFunction(message) == WRITE_COMPLETE;
	}

	/*std::vector<std::string> KawaiK3::presetNames()
	{
		return { (boost::format("Knobkraft %s %d") % getName() % channel().toOneBasedInt()).str() };
	}


	int getP(KawaiK3Parameter::Parameter param, Patch const &patch) {
		auto synthParam = KawaiK3Parameter::findParameter(param);
		if (synthParam != nullptr) {
			return patch.value(*synthParam);
		}
		jassert(false);
		return 0;
	}

	std::string getPasText(KawaiK3Parameter::Parameter param, Patch const &patch) {
		auto synthParam = KawaiK3Parameter::findParameter(param);
		if (synthParam != nullptr) {
			auto value = patch.value(*synthParam);
			return synthParam->valueAsText(value);
		}
		jassert(false);
		return "error";
	}*/

	MidiChannel KawaiK3::getInputChannel() const
	{
		return channel();
	}

	juce::MidiMessage KawaiK3::patchToSysex(Synth::PatchData const &patch, int programNo) const
	{
		if (programNo < 0 || programNo > 101) {
			jassert(false);
			return MidiMessage();
		}

		// This is just the reverse nibbling for that patch data...
		// Works for the wave as well
		std::vector<uint8> data = buildSysexFunction(ONE_BLOCK_DATA_DUMP, (uint8)programNo);
		uint8 sum = 0;
		for (auto item : patch) {
			sum += item;
			data.push_back((item & 0xf0) >> 4);
			data.push_back(item & 0x0f);
		}
		data.push_back((sum & 0xf0) >> 4);
		data.push_back(sum & 0x0f);
		return MidiMessage::createSysExMessage(&data[0], static_cast<int>(data.size()));
	}

	void KawaiK3::selectRegistration(Patch *currentPatch, DrawbarOrgan::RegistrationDefinition selectedRegistration)
	{
		selectHarmonics(currentPatch, selectedRegistration.first, selectedRegistration.second);
	}

	void KawaiK3::selectHarmonics(Patch *currentPatch, std::string const &name, Additive::Harmonics const &selectedHarmonics)
	{
		auto samples = Additive::createSamplesFromHarmonics(selectedHarmonics);
		//wave1_.displaySampledWave(samples);
		auto userWave = waveToSysex(selectedHarmonics);
		SimpleLogger::instance()->postMessage((boost::format("Sending user wave for registration %s to K3") % name).str());
		ignoreUnused(currentPatch);
		//sendK3Wave(MidiController::instance(), MidiController::instance(), SimpleLogger::instance(), currentPatch, userWave);
	}

	void KawaiK3::sendPatchToSynth(MidiController *controller, SimpleLogger *logger, std::shared_ptr<DataFile> dataFile) {
		MidiBuffer messages;
		messages.addEvent(patchToSysex(dataFile->data(), kFakeEditBuffer.toZeroBased()), 0);
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
		logger->postMessage((boost::format("Writing K3 patch to program %s") % (KawaiK3PatchNumber(kFakeEditBuffer).friendlyName())).str());
		controller->enableMidiOutput(midiOutput());
		controller->enableMidiInput(midiInput());
		controller->getMidiOutput(midiOutput())->sendBlockOfMessagesNow(messages);
	}

/*	void KawaiK3::retrieveWave(MidiController *controller, EditBufferHandler *handler, SimpleLogger *logger, SynthView *synthView) {
		controller->enableMidiInput(midiInput());
		auto handle = EditBufferHandler::makeOne();
		KawaiK3::WaveType waveType = KawaiK3::WaveType::USER_WAVE;
		handler->setNextEditBufferHandler(handle, [handler, handle, logger, controller, synthView, this](const juce::MidiMessage &message) {
			if (isWaveBufferDump(message)) {
				auto wave = waveFromSysex(message);
				KawaiK3::WaveType type = static_cast<KawaiK3::WaveType>(message.getSysExData()[5]);
				logger->postMessage((boost::format("Received %s from K3") % waveName(type)).str());

				auto samples = Additive::createSamplesFromHarmonics(wave);

				if (type == KawaiK3::WaveType::USER_WAVE_CARTRIDGE) {
					synthView->displaySampledWave(1, samples);
					// Done
					handler->removeEditBufferHandler(handle);
				}
				else {
					synthView->displaySampledWave(0, samples);

					// Request next wave
					type = static_cast<KawaiK3::WaveType>(((int)type) + 1);
					logger->postMessage((boost::format("Requesting %s from K3") % waveName(type)).str());
					auto requestMessage = requestWaveBufferDump(type);
					controller->getMidiOutput(midiOutput())->sendMessageNow(requestMessage);
				}
			}
		});
		// Send request for user wave to K3
		logger->postMessage((boost::format("Requesting %s from K3") % waveName(waveType)).str());
		auto requestMessage = requestWaveBufferDump(waveType);
		controller->getMidiOutput(midiOutput())->sendMessageNow(requestMessage);
	}

	void KawaiK3::sendK3Wave(MidiController *controller, EditBufferHandler *handler, SimpleLogger *logger, Patch *currentPatch, MidiMessage const &userWave)
	{
		auto handle = EditBufferHandler::makeOne();
		handler->setNextEditBufferHandler(handle, [this, handle, controller, handler, logger, currentPatch](MidiMessage const &message) {
			// Is this the write confirmation we are waiting for?
			if (isWriteConfirmation(message)) {
				handler->removeEditBufferHandler(handle);
				logger->postMessage("Got wave write confirmation from K3");
				if (dynamic_cast<KawaiK3Patch *>(currentPatch) != nullptr) {
					// Interesting, the patch is a KawaiK3 patch - send it to the K3!
					// Now... if the current patch is known and uses at least one user waveform oscillator, we want to make sure it is stored 
					// in the K3 and then issue a program change so the new user wave gets picked up!
					//TODO - optimize and don't send when no oscillator uses user wave
					sendPatchToSynth(controller, handler, logger, *currentPatch);
				}
			}
		});
		controller->enableMidiInput(midiInput());
		controller->getMidiOutput(midiOutput())->sendMessageNow(userWave);
	}*/


	/*void KawaiK3::setupBCR2000(MidiController *controller, BCR2000 &bcr, SimpleLogger *logger) {
		if (!bcr.channel().isValid()) return;
		if (!channel().isValid()) return;

		// Use MIDI channel 16 to not interfere with any other MIDI hardware accidentally taking the CC messages for real
		auto bcl = KawaiK3BCR2000::generateBCL(presetNames()[0], MidiChannel::fromOneBase(16), channel());
		auto syx = bcr.convertToSyx(bcl);
		controller->enableMidiInput(bcr.midiInput()); // Make sure we listen to the answers from the BCR2000 that we detected!
		bcr.sendSysExToBCR(controller->getMidiOutput(bcr.midiOutput()), syx, *controller, logger);
	}

	void KawaiK3::syncDumpToBCR(MidiProgramNumber programNumber, MidiController *controller, BCR2000 &bcr, SimpleLogger *logger) {
		if (k3BCRSyncHandler_.is_nil()) {
			k3BCRSyncHandler_ = EditBufferHandler::makeOne();
			controller->setNextEditBufferHandler(k3BCRSyncHandler_, [this, logger, controller, &bcr](MidiMessage const &message) {
				if (isSingleProgramDump(message)) {
					logger->postMessage("Got Edit Buffer from K3 and now updating the BCR2000");
					currentPatch_ = std::dynamic_pointer_cast<KawaiK3Patch>(patchFromProgramDumpSysex(message));

					if (currentPatch_) {
						// First update the Knobkraft UI - we can e.g. set the envelopes properly now
						//MessageManager::callAsync([this]() { synthView_->updatePatchDisplay(k3_.patchToSynthSetup(currentPatch_->data())); });

						// Loop over all parameters in patch and send out the appropriate CCs, if they are within the BCR2000 definition...
						std::vector<MidiMessage> updates;
						for (auto paramDef : KawaiK3Parameter::allParameters) {
							logger->postMessage((boost::format("Setting %s [%d] to %d") % paramDef->name() % paramDef->paramNo() % currentPatch_->value(*paramDef)).str());
							auto ccMessage = KawaiK3BCR2000::createMessageforParam(paramDef, *currentPatch_, MidiChannel::fromOneBase(16));
							updates.push_back(ccMessage);
						}
						MidiBuffer updateBuffer = MidiHelpers::bufferFromMessages(updates);
						// Send this to BCR
						if (bcr.channel().isValid() && controller->getMidiOutput(bcr.midiOutput())) {
							controller->getMidiOutput(bcr.midiOutput())->sendBlockOfMessagesNow(updateBuffer);
						}
					}
					else {
						logger->postMessage("Failure reading sysex stream");
					}
				}
			});
		}
		// We want to talk to the Synth now
		controller->enableMidiInput(midiInput());
		MidiBuffer requestBuffer = MidiHelpers::bufferFromMessages(requestPatch(programNumber.toZeroBased()));
		controller->getMidiOutput(midiOutput())->sendBlockOfMessagesNow(requestBuffer);
	}

	void KawaiK3::setupBCR2000View(BCR2000_Component &view) {
		KawaiK3BCR2000::setupBCR2000View(view);
	}

	void KawaiK3::setupBCR2000Values(BCR2000_Component &view, Patch *patch)
	{
		KawaiK3BCR2000::setupBCR2000Values(view, patch);
	}*/

}
