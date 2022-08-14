/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "Virus.h"

#include "MidiHelpers.h"
#include "MidiController.h"

#include "VirusPatch.h"

#include <boost/format.hpp>

namespace midikraft {

	// Definition for the "unused" data bytes inside Page A and Page B of the Virus.
	// These unused bytes need to be blanked for us to compare patches in order to detect duplicates
	//TODO Warning - this is the table that is correct for the Virus B. But the Virus C, TI, and maybe even TI2 seem to use those bytes
	// for different purposes, so blanking all of this is probably too much.
	std::vector<Range<int>> kVirusBlankOutZones = {
		{ 0, 5 }, // general controllers in Page A
		{ 6, 10}, // more controllers
		{ 11, 17 }, // and more controllers
		{ 32, 33 }, // bank number
		{ 50, 51 }, // byte 50 is undocumented
		{ 92, 93 }, // byte 92 is undocumented
		{ 95, 97 }, // more unknown bytes
		{ 103, 105 }, // and more
		{ 111, 112 }, // 111 also unknown
		{ 120, 128 }, // and more data not relevant
		{ 128 + 0, 129 + 0 }, // 128 is 0 of Page B, and not documented
		{ 128 + 14, 128 + 16 }, // Two bytes in Page B that are undocumented
		{ 128 + 22, 128 + 25 },
		{ 128 + 29, 128 + 30 },
		{ 128 + 37, 128 + 38 },
		{ 128 + 40, 128 + 41 },
		{ 128 + 45, 128 + 47 },
		{ 128 + 51, 128 + 54 },
		{ 128 + 59, 128 + 60 },
		{ 128 + 83, 128 + 84 },
		{ 128 + 91, 128 + 97 },
		{ 128 + 102, 128 + 112 }, // Undocumented
		{ 128 + 112, 128 + 122 }, // Ten characters of Patch name, actually not relevant
		{ 128 + 123, 128 + 128}
	};

	Virus::Virus() : deviceID_(0x10) /* Device ID 0x10 is omni = all Viruses */
	{
	}

	bool Virus::isOwnSysex(MidiMessage const &message) const
	{
		return message.isSysEx()
			&& (message.getSysExDataSize() > 3)
			&& message.getSysExData()[0] == 0x00
			&& message.getSysExData()[1] == 0x20
			&& message.getSysExData()[2] == 0x33   // Access Music
			&& message.getSysExData()[3] == 0x01;  // Virus
	}

	int Virus::numberOfBanks() const
	{
		return 8;
	}

	std::vector<MidiMessage> Virus::requestEditBufferDump() const
	{
		std::vector<uint8> message({ 0x30 /* Single request */, 0x00 /* Single Buffer */, 0x40 /* Single mode single buffer */ });
		return { createSysexMessage(message) };
	}

	std::vector<juce::MidiMessage> Virus::requestPatch(int patchNo) const
	{
		jassert(patchNo >= 0 && patchNo < 1024);
		uint8 bank = (uint8)((patchNo / 128) + 1);
		uint8 program = (uint8)(patchNo % 128);
		jassert(bank >= 1 && bank <= 8);
		jassert(program < 128);
		std::vector<uint8> message({ 0x30 /* Single request */, bank, program });
		return std::vector<MidiMessage>({ createSysexMessage(message) });
	}

	std::vector<MidiMessage> Virus::requestBankDump(MidiBankNumber bankNo) const
	{
		jassert(bankNo.toOneBased() >= 1 && bankNo.toOneBased() <= 8);
		std::vector<uint8> message({ 0x32 /* Single-bank request */, (uint8)bankNo.toOneBased() /* single bank */ });
		return { createSysexMessage(message) };
	}

	juce::MidiMessage Virus::saveEditBufferToProgram(int programNumber)
	{
		ignoreUnused(programNumber);
		// That's not implemented, you would need to retrieve the buffer and then store it with a dump in the right place
		// Doesn't make any sense anyway, because of the Virus' multiple edit buffers
		return MidiMessage();
	}

	bool Virus::isEditBufferDump(const std::vector<MidiMessage>& message) const
	{
		// The Virus uses a single message for an edit buffer dump
		if (message.size() == 1 && isOwnSysex(message[0])) {
			if (message[0].getSysExData()[5] == 0x10 /* Single dump */) {
				uint8 bank = message[0].getSysExData()[6];
				uint8 program = message[0].getSysExData()[7];
				return bank == 0x00 && (program == 0x40 || program == 0x7f); // See requestEditBuffer. The 7f seems to appear with the Virus C?
			}
		}
		return false;
	}

	bool Virus::isSingleProgramDump(const std::vector<MidiMessage>& message) const
	{
		if (message.size() == 1 && isOwnSysex(message[0])) {
			if (message[0].getSysExData()[5] == 0x10 /* Single dump */) {
				uint8 bank = message[0].getSysExData()[6];
				return bank >= 0x01 && bank <= 0x08;
			}
		}
		return false;
	}

	MidiProgramNumber Virus::getProgramNumber(const std::vector<MidiMessage> &message) const
	{
		if (isSingleProgramDump(message)) {
			uint8 bank = message[0].getSysExData()[6];
			if (bank > 0) bank -= 1; // bank is 1-based, but in case of edit buffer this will be 0 already
			uint8 program = message[0].getSysExData()[7];
			return MidiProgramNumber::fromZeroBaseWithBank(MidiBankNumber::fromZeroBase(bank, numberOfPatches()), program);
		}
		return MidiProgramNumber::fromZeroBase(0);
	}

	std::shared_ptr<DataFile> Virus::patchFromProgramDumpSysex(const std::vector<MidiMessage>& messages) const
	{
		return patchFromSysex(messages);
	}

	std::vector<juce::MidiMessage> Virus::patchToProgramDumpSysex(std::shared_ptr<DataFile> patch, MidiProgramNumber programNumber) const
	{
		int bank = programNumber.toZeroBasedWithBank() / numberOfPatches();
		std::vector<uint8> message({ 0x10 /* Single program dump */, (uint8) (bank + 1) /* 1-based bank number */, (uint8) (programNumber.toZeroBasedWithBank()) /* Program Number 0 to 127 */ });
		std::copy(patch->data().begin(), patch->data().end(), std::back_inserter(message));
		uint8 checksum = (MidiHelpers::checksum7bit(message) + deviceID_) & 0x7f;
		message.push_back(checksum);
		return std::vector<MidiMessage>({ createSysexMessage(message) });		
	}

	bool Virus::isBankDump(const MidiMessage& message) const
	{
		if (isOwnSysex(message)) {
			if (message.getSysExData()[5] == 0x10 /* Single dump */) {
				uint8 bank = message.getSysExData()[6];
				return bank >= 0x01 && bank <= 0x08;
			}
		}
		return false;
	}

	bool Virus::isBankDumpFinished(std::vector<MidiMessage> const &bankDump) const
	{
		// Count the number of patch dumps in that stream
		int found = 0;
		for (auto message : bankDump) {
			if (isSingleProgramDump({ message })) found++;
		}
		return found == 128;
	}

	std::string Virus::friendlyBankName(MidiBankNumber bankNo) const
	{
		char bankChar(char(bankNo.toZeroBased() + 'A'));
		return (boost::format("Bank %c") % bankChar).str();
	}

	Synth::PatchData Virus::filterVoiceRelevantData(std::shared_ptr<DataFile> unfilteredData) const
	{
		// Phew, the Virus has lots of unused data bytes that contribute nothing to the sound of the patch
		// Just blank out those bytes
		return Patch::blankOut(kVirusBlankOutZones, unfilteredData->data());
	}

	int Virus::numberOfPatches() const
	{
		return 128;
	}

	TPatchVector Virus::patchesFromSysexBank(const MidiMessage& message) const
	{
		ignoreUnused(message);
		// Coming here would be a logic error - the Virus has a patch dump request, but the synth will reply with lots of individual patch dumps
		throw std::logic_error("The method or operation is not implemented.");
	}

	std::shared_ptr<DataFile> Virus::patchFromSysex(const std::vector <MidiMessage>& message) const
	{
		if (isEditBufferDump(message) || isSingleProgramDump(message)) {
			auto pages = getPagesFromMessage(message[0], 8);
			if (pages.size() == 256) {
				// That should be Page A and Page B from the manual
				MidiProgramNumber place;
				if (isSingleProgramDump(message)) {
					place = getProgramNumber(message);
				}
				auto patch = std::make_shared<VirusPatch>(pages, place);
				return patch;
			}
		}
		return std::shared_ptr<Patch>();
	}

	std::shared_ptr<DataFile> Virus::patchFromPatchData(const Synth::PatchData &data, MidiProgramNumber place) const
	{
		auto patch = std::make_shared<VirusPatch>(data, place);
		return patch;
	}

	std::vector<juce::MidiMessage> Virus::patchToSysex(std::shared_ptr<DataFile> patch) const
	{
		std::vector<uint8> message({ 0x10 /* Single program dump */, 0x00 /* Edit Buffer */, 0x40 /* Single Buffer */ });
		std::copy(patch->data().begin(), patch->data().end(), std::back_inserter(message));
		uint8 checksum = (MidiHelpers::checksum7bit(message) + deviceID_) & 0x7f;
		message.push_back(checksum);
		return std::vector<MidiMessage>({ createSysexMessage(message) });
	}

	std::string Virus::getName() const
	{
		return "Access Virus B";
	}

	std::vector<juce::MidiMessage> Virus::deviceDetect(int channel)
	{
		ignoreUnused(channel);
		// Send a global request, from which we can read the device ID and the global channel of the Virus
		return { createSysexMessage(std::vector<uint8>({ 0x35 /* Global request */ })) };
	}

	int Virus::deviceDetectSleepMS()
	{
		// Better be safe than sorry, the global request is not processed quickly by the Virus. This is the slowest synth I have encountered so far
		return 150;
	}

	MidiChannel Virus::channelIfValidDeviceResponse(const MidiMessage &message)
	{
		if (isOwnSysex(message)) {
			if (message.getSysExData()[5] == 0x12) {
				// Undocumented in the manual, but that seems to be the "Global Dump" package.
				//uint8 bb = message.getSysExData()[6];
				//uint8 mm = message.getSysExData()[7];

				auto pages = getPagesFromMessage(message, 8);
				if (pages.size() == 256) {
					// Wonderful, we should have 2 pages as response to the global dump request: 
					// page 0 is the Page C from the manual containing global data, like the global Virus channel
					// page 1 is - at least in my Virus B - a page containing nothing but the numbers from 0 to 127
					// at index 124 we will find the global channel (0-based)
					deviceID_ = pages[VirusPatch::index(VirusPatch::PageA, 93)];
					return MidiChannel::fromZeroBase(pages[VirusPatch::index(VirusPatch::PageA, 124)]);
				}
			}
		}
		return MidiChannel::invalidChannel();
	}

	std::vector<uint8> Virus::getPagesFromMessage(MidiMessage const &message, int dataStartIndex) const {
		if (isOwnSysex(message)) {
			// Extract the data block and check the checksum
			std::vector<uint8> data;
			int sum = 0;
			for (int additionalChecksummed = -4; additionalChecksummed < 0; additionalChecksummed++) {
				sum += message.getSysExData()[dataStartIndex + additionalChecksummed];
			}
			for (int i = dataStartIndex; i < message.getSysExDataSize() - 1; i++) {
				uint8 byte = message.getSysExData()[i];
				sum += byte;
				data.push_back(byte);
			}
			if ((sum & 0x7f) == message.getSysExData()[message.getSysExDataSize() - 1]) {
				// CRC passed, we have a real data package
				return data;
			}
			else {
				// ouch
				Logger::writeToLog("CRC checksum error when decoding Virus patch data");
			}
		}
		return std::vector<uint8>();
	}

	bool Virus::needsChannelSpecificDetection()
	{
		return false;
	}

	bool Virus::canChangeInputChannel() const
	{
		return true;
	}

	void Virus::changeInputChannel(MidiController *controller, MidiChannel channel, std::function<void()> finished)
	{
		// We're changing the global channel here. This is parameter 124 in Page C
		controller->getMidiOutput(midiOutput())->sendMessageNow(createParameterChangeSingle(2, 124, (uint8) channel.toZeroBasedInt()));
		// Assume this worked and set our internal channel anew 
		setCurrentChannelZeroBased(midiInput(), midiOutput(), channel.toZeroBasedInt());
	}

	MidiChannel Virus::getInputChannel() const
	{
		return channel();
	}

	bool Virus::hasMidiControl() const
	{
		return false;
	}

	bool Virus::isMidiControlOn() const
	{
		return true;
	}

	void Virus::setMidiControl(MidiController *controller, bool isOn)
	{
		ignoreUnused(controller, isOn);
		throw new std::runtime_error("Invalid call");
	}

	std::string Virus::friendlyProgramName(MidiProgramNumber programNumber) const
	{
		char bankChar = 'a';
		if (programNumber.isBankKnown()) {
			bankChar = char(programNumber.bank().toZeroBased() + 'a');
		}
		int progNo = programNumber.toZeroBasedWithBank();
		return (boost::format("%c%d") % bankChar % progNo).str();
	}

	MidiMessage Virus::createSysexMessage(std::vector<uint8> const &message) const {
		jassert(deviceID_ >= 0 && deviceID_ < 0x80);
		std::vector<uint8> messageFrame({ 0x00, 0x20, 0x33 /* Access Music */, 0x01 /* Virus */, deviceID_ });
		std::copy(message.begin(), message.end(), std::back_inserter(messageFrame));
		return MidiHelpers::sysexMessage(messageFrame);
	}

	MidiMessage Virus::createParameterChangeSingle(int page, int paramNo, uint8 value) const {
		jassert(page >= 0 && page <= 2); // A, B, or global
		jassert(paramNo >= 0 && paramNo < 128);
		jassert(value >= 0 && value < 128);
		std::vector<uint8> message({ (uint8)(0x70 + page), 0x40 /* Single buffer */, (uint8)paramNo, value });
		return createSysexMessage(message);
	}

}
