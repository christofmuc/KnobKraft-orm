#include "Virus.h"

#include "MidiHelpers.h"
#include "SynthSetup.h"
#include "MidiController.h"

#include "VirusPatch.h"

// Definition for the "unused" data bytes inside Page A and Page B of the Virus.
// These unused bytes need to be blanked for us to compare patches in order to detect duplicates
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

juce::MidiMessage Virus::requestEditBufferDump()
{
	std::vector<uint8> message({ 0x30 /* Single request */, 0x00 /* Single Buffer */, 0x40 /* Single mode single buffer */ });
	return createSysexMessage(message);
}

std::vector<juce::MidiMessage> Virus::requestPatch(int patchNo)
{
	jassert(patchNo >= 0 && patchNo < 1024);
	uint8 bank = (uint8)((patchNo / 128) + 1);
	uint8 program = (uint8)(patchNo % 128);
	jassert(bank >= 1 && bank <= 8);
	jassert(program < 128);
	std::vector<uint8> message({ 0x30 /* Single request */, bank, program });
	return std::vector<MidiMessage>({ createSysexMessage(message) });
}

juce::MidiMessage Virus::requestBankDump(MidiBankNumber bankNo) const
{
	jassert(bankNo.toOneBased() >= 1 && bankNo.toOneBased() <= 8);
	std::vector<uint8> message({ 0x32 /* Single-bank request */, (uint8)bankNo.toOneBased() /* single bank */ });
	return createSysexMessage(message);
}

juce::MidiMessage Virus::saveEditBufferToProgram(int programNumber)
{
	// That's not implemented, you would need to retrieve the buffer and then store it with a dump in the right place
	// Doesn't make any sense anyway, because of the Virus' multiple edit buffers
	return MidiMessage();
}

bool Virus::isEditBufferDump(const MidiMessage& message) const
{
	if (isOwnSysex(message)) {
		if (message.getSysExData()[5] == 0x10 /* Single dump */) {
			uint8 bank = message.getSysExData()[6];
			uint8 program = message.getSysExData()[7];
			return bank == 0x00 && (program == 0x40 || program == 0x7f); // See requestEditBuffer. The 7f seems to appear with the Virus C?
		}
	}
	return false;
}

bool Virus::isSingleProgramDump(const MidiMessage& message) const
{
	if (isOwnSysex(message)) {
		if (message.getSysExData()[5] == 0x10 /* Single dump */) {
			uint8 bank = message.getSysExData()[6];
			return bank >= 0x01 && bank <= 0x08;
		}
	}
	return false;
}

std::shared_ptr<Patch> Virus::patchFromProgramDumpSysex(const MidiMessage& message) const
{
	return patchFromSysex(message);
}

std::vector<juce::MidiMessage> Virus::patchToProgramDumpSysex(const Patch &patch) const
{
	//TODO this might be too much of a shortcut
	return patchToSysex(patch);
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
		if (isSingleProgramDump(message)) found++;
	}
	return found == 128;
}

std::string Virus::friendlyBankName(MidiBankNumber bankNo) const
{
	char bankChar(char(bankNo.toZeroBased() + 'A'));
	return (boost::format("Bank %c") % bankChar).str();
}

Synth::PatchData Virus::filterVoiceRelevantData(Synth::PatchData const &unfilteredData) const
{
	// Phew, the Virus has lots of unused data bytes that contribute nothing to the sound of the patch
	// Just blank out those bytes
	return Patch::blankOut(kVirusBlankOutZones, unfilteredData);
}

int Virus::numberOfPatches() const
{
	return 128;
}

TPatchVector Virus::patchesFromSysexBank(const MidiMessage& message) const
{
	// Coming here would be a logic error - the Virus has a patch dump request, but the synth will reply with lots of individual patch dumps
	throw std::logic_error("The method or operation is not implemented.");
}

std::shared_ptr<Patch> Virus::patchFromSysex(const MidiMessage& message) const
{
	if (isEditBufferDump(message) || isSingleProgramDump(message)) {
		auto pages = getPagesFromMessage(message, 8);
		if (pages.size() == 256) {
			// That should be Page A and Page B from the manual
			auto patch = std::make_shared<VirusPatch>(pages);
			return patch;
		}
	}
	return std::shared_ptr<Patch>();
}

std::shared_ptr<Patch> Virus::patchFromPatchData(const Synth::PatchData &data, std::string const &name, MidiProgramNumber place) const
{
	auto patch = std::make_shared<VirusPatch>(data);
	patch->setPatchNumber(place);
	return patch;
}

std::vector<juce::MidiMessage> Virus::patchToSysex(const Patch &patch) const
{
	std::vector<uint8> message({ 0x10 /* Single program dump */, 0x00 /* Edit Buffer */, 0x40 /* Single Buffer */ });
	std::copy(patch.data().begin(), patch.data().end(), std::back_inserter(message));
	uint8 checksum = (MidiHelpers::checksum7bit(message) + deviceID_) & 0x7f;
	message.push_back(checksum);
	return std::vector<MidiMessage>({ createSysexMessage(message) });
}

std::string Virus::getName() const
{
	return "Access Virus B";
}

juce::MidiMessage Virus::deviceDetect(int channel)
{
	// Send a global request, from which we can read the device ID and the global channel of the Virus
	return createSysexMessage(std::vector<uint8>({ 0x35 /* Global request */ }));
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
			uint8 bb = message.getSysExData()[6];
			uint8 mm = message.getSysExData()[7];

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

void Virus::changeInputChannel(MidiController *controller, MidiChannel channel)
{
	// We're changing the global channel here. This is parameter 124 in Page C
	controller->getMidiOutput(midiOutput())->sendMessageNow(createParameterChangeSingle(2, 124, channel.toZeroBasedInt()));
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
	throw new std::runtime_error("Invalid call");
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


