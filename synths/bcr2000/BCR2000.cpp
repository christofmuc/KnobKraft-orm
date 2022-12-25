/*
   Copyright (c) 2019 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "BCR2000.h"

#include "BCRDefinition.h"
#include "MidiHelpers.h"
#include "Sysex.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <iterator> 

#include <fmt/format.h>
#include <spdlog/spdlog.h>
#include <map>
#include <regex>

namespace {

	// https://stackoverflow.com/questions/216823/how-to-trim-an-stdstring
	// trim from start (in place)
	static inline void ltrim(std::string& s) {
		s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
			return !std::isspace(ch);
			}));
	}

	// trim from end (in place)
	static inline void rtrim(std::string& s) {
		s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
			return !std::isspace(ch);
			}).base(), s.end());
	}

	// trim from both ends (in place)
	static inline void trim(std::string& s) {
		ltrim(s);
		rtrim(s);
	}

}

namespace midikraft {

	std::map<int, std::string> errorNames = {
		{ 0, "no error" },
		{ 1, "unknown token" },
		{ 2, "data without token" },
		{ 3, "argument missing" },
		{ 4, "wrong device" },
		{ 5, "wrong revision" },
		{ 6, "missing revision" },
		{ 7, "internal error" },
		{ 8, "mode missing" },
		{ 9, "bad item index" },
		{ 10, "not a number" },
		{ 11, "value out of range"},
		{ 12, "invalid argument"},
		{ 13, "invalid command"},
		{ 14, "wrong number of arguments"},
		{ 15, "too much data"},
		{ 16, "already defined"},
		{ 17, "preset missing"},
		{ 18, "preset too complex"},
		{ 19, "wrong preset"},
		{ 20, "preset too new"},
		{ 21, "preset check"},
		{ 22, "sequence error" },
		{ 23, "wrong context"}
	};

	enum BCR2000Commands {
		REQUEST_IDENTITY = 0x01,
		SEND_IDENTITY = 0x02,
		SEND_BCL_MESSAGE = 0x20,
		BCL_REPLY = 0x21,
		SELECT_PRESET = 0x22,
		SEND_FIRMWARE = 0x34,
		FIRMWARE_REPLY = 0x35,
		REQUEST_DATA = 0x40,
		REQUEST_GLOBAL_SETUP = 0x41,
		REQUEST_PRESET_NAME = 0x42,
		REQUEST_SNAPSHOT = 0x43,
		SEND_TEXT = 0x78
	};

	void BCR2000::writeToFile(std::string const &filename, std::string const &bcl) const
	{
		std::ofstream bcrFile;
		bcrFile.open(filename);
		bcrFile << bcl;
		bcrFile.close();
	}

	std::vector<juce::MidiMessage> BCR2000::convertToSyx(std::string const &bcl, bool verbatim /* = false */) const
	{
		std::vector<MidiMessage> result;

		// This is a first - we actually send the program text in BCL clear text format to the device
		// wrapped in messages not longer than 512 byte
		std::vector<uint8> sysEx = createSysexCommandData(SEND_BCL_MESSAGE);

		// Looping over the source code, chunking it into as many MIDI messages as required
		std::stringstream input(bcl);
		int maxLen = 500;
		uint16 messageNo = 0;
		while (!input.eof()) {
			std::vector<uint8> characters;
			std::string line;
			std::getline(input, line, '\n');

			if (!verbatim) {
				// Strip comments to accelerate transmission
				line = line.substr(0, line.find(";", 0));
				rtrim(line);

				// No need to send empty lines to BCR
				if (line.empty())
					continue;
			}
			else {
				// Only trim - this is important for \r\n to \n conversion
				rtrim(line);
			}

			for (auto c : line) characters.push_back(c);

			// First sanitize characters...
			for (uint8 &value : characters) {
				if (value < 32 || value > 127) {
					// Shouldn't happen
					jassert(false);
					value = '_';
				}
			}
			// Truncate lines longer than maxLen characters
			if (characters.size() > maxLen) {
				characters.resize(maxLen);
			}

			std::vector<uint8> message;
			std::copy(sysEx.begin(), sysEx.end(), std::back_inserter(message));

			// Write sequential number (14 bits)
			message.push_back((messageNo >> 7) & 0x7f);
			message.push_back(messageNo & 0x7f);
			messageNo++;

			std::copy(characters.begin(), characters.end(), std::back_inserter(message));
			result.push_back(MidiMessage::createSysExMessage(&message[0], (int)message.size()));
		}
		return result;
	}

	std::vector<uint8> BCR2000::createSysexCommandData(uint8 commandCode) const {
		return std::vector<uint8>({ 0x00, 0x20, 0x32 /* the first three bytes are Behringer */,  0x7f /* any device ID */, 0x15 /* That's the BCR2000 */, commandCode });
	}

	std::string BCR2000::convertSyxToText(const MidiMessage &message) {
		if (isSysexFromBCR2000(message)) {
			auto data = message.getSysExData();
			if (data[5] == SEND_BCL_MESSAGE) {
				// This is a BCL message which contains actually only a line of text in 7bit ASCII
				uint16 lineno = data[6] << 7 | data[7];
				ignoreUnused(lineno);
				std::string result;
				for (int rest = 8; rest < message.getSysExDataSize(); rest++) {
					result.push_back(data[rest]);
				}
				return result;
			}
		}
		jassert(false);
		return "";
	}

	std::string BCR2000::findPresetName(std::vector<MidiMessage> const &messages) const {
		// Find the first line which states ".name"
		const std::regex base_regex(R"([[:space:]]*\.name[[:space:]]+'([^']*)')", std::regex_constants::extended| std::regex_constants::icase);
		std::smatch base_match;
		for (auto message : messages) {
			auto line = convertSyxToText(message);
			if (std::regex_match(line, base_match, base_regex)) {
				// The second submatch is the expression, the first one the whole string that matches
				if (base_match.size() == 2) {
					std::ssub_match base_sub_match = base_match[1];
					return base_sub_match;
				}
			}
		}
		return "Unknown Preset";
	}

	bool BCR2000::isSysexFromBCR2000(const MidiMessage& message)
	{
		auto data = message.getSysExData();

		if (message.getSysExDataSize() < 5)
			// Too short for anything BCR2000
			return false;

		//auto deviceID = 0x7f;
		return message.isSysEx() && data[0] == 0x00 && data[1] == 0x20 && data[2] == 0x32 /* Behringer */
			// Ignore for now && data[3] == deviceID
			&& (data[4] == 0x15 || data[4] == 0x7f); // is for BCR2000 (or generic)
	}

	uint8 BCR2000::sysexCommand(const MidiMessage &message) const {
		return message.getSysExData()[5];
	}

	void BCR2000::sendSysExToBCR(std::shared_ptr<SafeMidiOutput> midiOutput, std::vector<MidiMessage> const &messages, std::function<void(std::vector<BCRError> const &errors)> const whenDone)
	{
		errorsDuringUpload_.clear();
		TransferCounters *receivedCounter = new TransferCounters;
		// Determine what we will do with the answer...
		auto handle = MidiController::makeOneHandle();
		std::vector<MidiMessage> localCopy = messages;
		MidiController::instance()->addMessageHandler(handle, [this, localCopy, midiOutput, receivedCounter, handle, whenDone](MidiInput *source, const juce::MidiMessage &answer) {
			if (source->getDeviceInfo() != midiInput()) return;

			// Check the answer from the BCR2000
			if (isSysexFromBCR2000(answer)) {
				// Is this a reply to our BCL message?
				if (answer.getSysExDataSize() == 9) {
					auto data = answer.getSysExData();
					if (data[5] == BCL_REPLY) {
						// Command code is 0x21 and data size is 9, this is the answer we have been waiting for
						uint16 lineNo = data[6] << 7 | data[7];
						uint8 error = data[8];

						// Check for dropped messages...
						int logicalLineNumber = receivedCounter->overflowCounter * (1 << 14) + lineNo;
						if (logicalLineNumber != receivedCounter->receivedMessages && receivedCounter->lastLine != -1) {
							spdlog::warn("BCR2000: Seems to have a MIDI message drop in communication");
						}
						if (lineNo < receivedCounter->lastLine && lineNo == 0) {
							// That was a wrap around
							receivedCounter->overflowCounter++;
						}

						if (error != 0) {
							std::string errorText = "unknown error";
							if (errorNames.find(error) != errorNames.end()) {
								errorText = errorNames[error];
							}
							if (logicalLineNumber >= 0 && logicalLineNumber < localCopy.size()) {
								auto currentLine = convertSyxToText(localCopy[logicalLineNumber]);
								errorsDuringUpload_.push_back({ error, errorText, logicalLineNumber + 1, currentLine });
								spdlog::error(errorsDuringUpload_.back().toDisplayString());
							}
							else {
								spdlog::error("Error {} ({}) in line {}", error, errorText, (logicalLineNumber + 1));
							}
						}

						// Check if we are done with the upload. As this wraps when we send more than 16000 lines, we need to make sure we realize that this happened
						receivedCounter->receivedMessages++;
						if (receivedCounter->receivedMessages == receivedCounter->numMessages - 1) {
							delete receivedCounter;
							MidiController::instance()->removeMessageHandler(handle);
							spdlog::info("All messages received by BCR2000");
							whenDone(errorsDuringUpload_);
						}
						else {
							receivedCounter->lastLine = lineNo; // To detect the wrap around
							midiOutput->sendMessageNow(localCopy[receivedCounter->lastLine + 1]);
						}
					}
				}
			}
			// Ignore all other messages
		});

		// Send all messages immediately
		if (messages.size() > 0) {
			receivedCounter->numMessages = (int) messages.size();
			receivedCounter->receivedMessages = 0;
			receivedCounter->lastLine = -1;
			receivedCounter->overflowCounter = 0;
			if (midiOutput != nullptr) {
				midiOutput->sendMessageNow(messages[0]);
			}
			else {
				spdlog::warn("No Midi Output known for BCR2000, not sending anything!");
			}
		}
		else {
			jassert(false);
		}
	}

	std::string BCR2000::getName() const
	{
		return "Behringer BCR2000";
	}

	std::vector<juce::MidiMessage> BCR2000::deviceDetect(int /* channel */) // The BCR can not detect on a specific channel, but will reply on all of them as it uses sysex
	{
		return { MidiHelpers::sysexMessage(createSysexCommandData(REQUEST_IDENTITY)) };
	}

	int BCR2000::deviceDetectSleepMS()
	{
		// 100 ms always worked for me
		return 100;
	}

	MidiChannel BCR2000::channelIfValidDeviceResponse(const MidiMessage &message)
	{
		if (isSysexFromBCR2000(message)) {
			// This is the reply. We do not use the identity string returned
			if (message.getSysExDataSize() >= 5 && sysexCommand(message) == SEND_IDENTITY) {
				return MidiChannel::fromZeroBase(0); // I think this flags only that the BCR was detected successfully - as we talk to it only via sysex, it has no "channel"
			}
		}
		return MidiChannel::invalidChannel();
	}

	bool BCR2000::needsChannelSpecificDetection()
	{
		// The BCR uses sysex, and will reply on all channels with a Device ID
		return false;
	}

	std::string BCR2000::generateBCRHeader() {
		return "; Generated by KnobKraft\n"
			"\n"
			"$rev R1\n"
			"$global\n"
			"  .rxch off\n";
	}

	std::string BCR2000::generateBCREnd(int recallPreset) {
		if (recallPreset >= 1 && recallPreset <= 32) {
			return fmt::format("$recall {}\n$end\n", recallPreset);
		}
		else {
			return "$end\n";
		}
	}

	std::string BCR2000::generatePresetHeader(std::string const &presetName) {
		jassert(presetName.size() < 24);
		return fmt::format("$preset\n"
			"  .name '{}'\n"
			"  .snapshot off\n"
			"  .request off\n"
			"  .fkeys on\n"
			"  .egroups 4\n"
			"  .lock off\n"
			"  .init\n", presetName);
	}

	std::string BCR2000::generateBCRFooter(int storagePlace) {
		if (storagePlace >= 1 && storagePlace <= 32) {
			return fmt::format("$store {}\n", storagePlace);
		}
		return "";
	}

	std::string BCR2000::generateAllEncoders(std::vector<std::pair<BCRdefinition *, std::string>> &all_entries) {
		std::string result;

		// Generate the sorted list - sorted by number, but encoders before buttons
		std::sort(all_entries.begin(), all_entries.end(), [](std::pair<BCRdefinition *, std::string> &entry1, std::pair<BCRdefinition *, std::string> &entry2) {
			if (entry1.first->encoderNumber() != entry2.first->encoderNumber()) {
				return entry1.first->encoderNumber() < entry2.first->encoderNumber();
			}
			else {
				return entry1.first->type() < entry2.first->type();
			}
		});

		// Then build the result string
		for (auto entry : all_entries) {
			result += entry.second;
		}

		return result;
	}

	std::string BCR2000::syxToBCRString(MidiMessage const &syx)
	{
		std::string result;
		if (syx.isSysEx()) {
			auto data = syx.getSysExData();
			for (int i = 0; i < syx.getSysExDataSize(); i++) {
				std::string formattedValue = fmt::format("${:02x} ", ((int)data[i]));
				result += formattedValue;
			}
		}
		return result;
	}

	std::vector<std::string> BCR2000::listOfPresets() const
	{
		return bcrPresets_;
	}

	int BCR2000::indexOfPreset(std::string const &name) const
	{
		for (int i = 0; i < bcrPresets_.size(); i++) {
			if (bcrPresets_[i] == name) {
				return i;
			}
		}
		return -1;
	}

	void BCR2000::selectPreset(MidiController *controller, int presetIndex)
	{
		auto command = createSysexCommandData(SELECT_PRESET);
		command.push_back((uint8)presetIndex);
		controller->getMidiOutput(midiOutput())->sendMessageNow(MidiHelpers::sysexMessage(command));
	}

	void BCR2000::refreshListOfPresets(std::function<void()> callback)
	{
		if (wasDetected()) {
			if (bcrPresets_.empty()) {
				auto myhandle = MidiController::makeOneHandle();
				MidiController::instance()->addMessageHandler(myhandle, [this, myhandle, callback](MidiInput *source, MidiMessage const &message) {
					ignoreUnused(source);
					if (isSysexFromBCR2000(message)) {
						if (sysexCommand(message) == BCL_REPLY && message.getSysExDataSize() == 32) {
							int presetNum = message.getSysExData()[7] + 1;
							std::string presetName(&message.getSysExData()[8], &message.getSysExData()[32]);
							trim(presetName);
							spdlog::debug("Preset #{}: {}", presetNum, presetName);
							bcrPresets_.push_back(presetName);
							if (presetNum == 32) {
								MidiController::instance()->removeMessageHandler(myhandle);
								callback();
							}
						}
					}
				});
				std::vector<uint8> requestNames = createSysexCommandData(REQUEST_PRESET_NAME);
				requestNames.push_back(0x7e); // That's for all names
				MidiController::instance()->getMidiOutput(midiOutput())->sendMessageNow(MidiHelpers::sysexMessage(requestNames));
			}
			else {
				// Nothing to do
				callback();
			}
		}
	}

	void BCR2000::invalidateListOfPresets()
	{
		bcrPresets_.clear();
	}

	MidiMessage BCR2000::requestEditBuffer() const
	{
		return requestStreamElement(0x7F, StreamLoadCapability::StreamType::EDIT_BUFFER_DUMP)[0];
	}

	std::vector<MidiMessage> BCR2000::requestStreamElement(int number, StreamLoadCapability::StreamType streamType) const
	{
		std::vector<uint8> data = createSysexCommandData(REQUEST_DATA);
		switch (streamType)
		{
		case midikraft::StreamLoadCapability::StreamType::EDIT_BUFFER_DUMP:
			data.push_back((uint8)0x7f);
			break;
		case midikraft::StreamLoadCapability::StreamType::BANK_DUMP:
			data.push_back((uint8)number);
			break;
		default:
			return {};
		}
		return { MidiHelpers::sysexMessage(data) };
	}

	int BCR2000::numberOfStreamMessagesExpected(StreamType streamType) const
	{
		ignoreUnused(streamType);
		return -1;
	}

	bool BCR2000::isMessagePartOfStream(const MidiMessage& message, StreamType streamType) const
	{
		ignoreUnused(streamType);
		if (isSysexFromBCR2000(message)) {
			// This is the reply. We do not use the identity string returned
			if (message.getSysExDataSize() >= 5 && sysexCommand(message) == SEND_BCL_MESSAGE) {
				return true;
			}
		}
		return false;
	}

	bool BCR2000::isStreamComplete(std::vector<MidiMessage> const &bankDump, StreamType streamType) const
	{
		ignoreUnused(streamType);
		if (!bankDump.empty()) {
			std::string line = convertSyxToText(bankDump.back());
			if (line == "$end") {
				return true;
			}
		}
		return false;
	}

	bool BCR2000::shouldStreamAdvance(std::vector<MidiMessage> const &messages, StreamType streamType) const
	{
		ignoreUnused(messages, streamType);
		return false;
	}

	midikraft::TPatchVector BCR2000::loadPatchesFromStream(std::vector<MidiMessage> const &streamDump) const
	{
		midikraft::TPatchVector result;

		// Find the name
		auto name = findPresetName(streamDump);

		std::vector<uint8> patchData;
		for (auto message : streamDump) {
			std::copy(message.getRawData(), message.getRawData() + message.getRawDataSize(), std::back_inserter(patchData));
		}
		result.push_back(std::make_shared<BCR2000Preset>(name, patchData));
		return result;
	}

	std::vector<juce::MidiMessage> BCR2000::dataFileToMessages(std::shared_ptr<DataFile> dataFile, std::shared_ptr<SendTarget> target) const
	{
		ignoreUnused(target);
		return Sysex::vectorToMessages(dataFile->data());
	}

	int BCR2000::numberOfBanks() const
	{
		return 32;
	}

	int BCR2000::numberOfPatches() const
	{
		return 1;
	}

	std::string BCR2000::friendlyBankName(MidiBankNumber bankNo) const
	{
		return fmt::format("Preset #{}", bankNo.toOneBased());
	}

	std::shared_ptr<midikraft::DataFile> BCR2000::patchFromPatchData(const Synth::PatchData &data, MidiProgramNumber place) const
	{
		ignoreUnused(place);
		auto allMessages = Sysex::vectorToMessages(data);
		auto name = findPresetName(allMessages);
		return std::make_shared<BCR2000Preset>(name, data);
	}

	bool BCR2000::isOwnSysex(MidiMessage const &message) const
	{
		return message.isSysEx() && isSysexFromBCR2000(message);
	}

	std::vector<juce::MidiMessage> BCR2000::dataFileToSysex(std::shared_ptr<DataFile> dataFile, std::shared_ptr<SendTarget> target)
	{
		ignoreUnused(target);
		return Sysex::vectorToMessages(dataFile->data());
	}

	void BCR2000::sendDataFileToSynth(std::shared_ptr<DataFile> dataFile, std::shared_ptr<SendTarget> target)
	{
		if (wasDetected()) {
			auto messages = dataFileToSysex(dataFile, target);
			sendSysExToBCR(MidiController::instance()->getMidiOutput(midiOutput()), messages, [](std::vector<BCRError> const &errors) {
				if (!errors.empty()) {
					spdlog::error("Preset contains errors");
				}
			});
		}
	}

	BCR2000Preset::BCR2000Preset(std::string const &name, Synth::PatchData const &data) : DataFile(0, data), name_(name)
	{		
	}

	std::string BCR2000Preset::name() const
	{
		return name_;
	}

	void BCR2000Preset::setName(std::string const& name) {
		juce::ignoreUnused(name);
		spdlog::error("Renaming BCR2000 presets is not implemented yet!");
	}

	std::string BCR2000::BCRError::toDisplayString() const
	{
		std::string sanitizedLine = lineText;
		sanitizedLine.erase(std::remove(sanitizedLine.begin(), sanitizedLine.end(), '\n'), sanitizedLine.end());
		return fmt::format("Error {} ({}) in line {}: {}", (int)errorCode, errorText, (lineNumber + 1), sanitizedLine);
	}

}
