/*
   Copyright (c) 2019 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "BCR2000.h"

#include "BCRDefinition.h"
#include "MidiHelpers.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <iterator> 

#include <boost/format.hpp>
#include <boost/algorithm/string.hpp>
#include <map>

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
				boost::trim_right(line);

				// No need to send empty lines to BCR
				if (line.empty())
					continue;
			}
			else {
				// Only trim - this is important for \r\n to \n conversion
				boost::trim_right(line);
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

	void BCR2000::sendSysExToBCR(SafeMidiOutput *midiOutput, std::vector<MidiMessage> const &messages, SimpleLogger *logger, std::function<void(std::vector<BCRError> const &errors)> const whenDone)
	{
		errorsDuringUpload_.clear();
		TransferCounters *receivedCounter = new TransferCounters;
		// Determine what we will do with the answer...
		auto handle = MidiController::makeOneHandle();
		MidiController::instance()->addMessageHandler(handle, [this, messages, midiOutput, logger, receivedCounter, handle, whenDone](MidiInput *source, const juce::MidiMessage &answer) {
			ignoreUnused(source);

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
						if (logicalLineNumber != receivedCounter->receivedMessages) {
							logger->postMessage("Seems to have a MIDI message drop");
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
							if (logicalLineNumber >= 0 && logicalLineNumber < messages.size()) {
								auto currentLine = convertSyxToText(messages[logicalLineNumber]);
								logger->postMessage((boost::format("Error %d (%s) in line %d: %s") % error % errorText % (logicalLineNumber + 1) % currentLine).str());
								errorsDuringUpload_.push_back({ error, errorText, logicalLineNumber + 1, currentLine });
							}
							else {
								logger->postMessage((boost::format("Error %d (%s) in line %d") % error % errorText % (logicalLineNumber + 1)).str());
							}
						}

						// Check if we are done with the upload. As this wraps when we send more than 16000 lines, we need to make sure we realize that this happened
						receivedCounter->receivedMessages++;
						if (receivedCounter->receivedMessages == receivedCounter->numMessages - 1) {
							delete receivedCounter;
							MidiController::instance()->removeMessageHandler(handle);
							logger->postMessage("All messages received by BCR2000");
							whenDone(errorsDuringUpload_);
						}
						else {
							receivedCounter->lastLine = lineNo; // To detect the wrap around
						}
					}
				}
			}
			// Ignore all other messages
		});

		// Send all messages immediately
		if (messages.size() > 0) {
			MidiBuffer buffer;
			int numMessages = 0;
			for (auto message : messages) {
				buffer.addEvent(message, numMessages++);
			}
			receivedCounter->numMessages = numMessages;
			receivedCounter->receivedMessages = 0;
			receivedCounter->lastLine = -1;
			receivedCounter->overflowCounter = 0;
			if (midiOutput != nullptr) {
				midiOutput->sendBlockOfMessagesNow(buffer);
			}
			else {
				logger->postMessage("No Midi Output known for BCR2000, not sending anything!");
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

	juce::MidiMessage BCR2000::deviceDetect(int /* channel */) // The BCR can not detect on a specific channel, but will reply on all of them as it uses sysex
	{
		return MidiHelpers::sysexMessage(createSysexCommandData(REQUEST_IDENTITY));
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
		return (boost::format("; Generated by KnobKraft\n"
			"\n"
			"$rev R1\n"
			"$global\n"
			"  .rxch off\n")).str();
	}

	std::string BCR2000::generateBCREnd(int recallPreset) {
		if (recallPreset >= 1 && recallPreset <= 32) {
			return (boost::format("$recall %d\n$end\n") % recallPreset).str();
		}
		else {
			return "$end\n";
		}
	}

	std::string BCR2000::generatePresetHeader(std::string const &presetName) {
		jassert(presetName.size() < 24);
		return (boost::format("$preset\n"
			"  .name '%s'\n"
			"  .snapshot off\n"
			"  .request off\n"
			"  .fkeys on\n"
			"  .egroups 4\n"
			"  .lock off\n"
			"  .init\n") % presetName).str();
	}

	std::string BCR2000::generateBCRFooter(int storagePlace) {
		if (storagePlace >= 1 && storagePlace <= 32) {
			return (boost::format("$store %d\n") % storagePlace).str();
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
				std::string formattedValue = (boost::format("$%02x ") % ((int)data[i])).str();
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
		if (bcrPresets_.empty()) {
			auto myhandle = MidiController::makeOneHandle();
			MidiController::instance()->addMessageHandler(myhandle, [this, myhandle, callback](MidiInput *source, MidiMessage const &message) {
				ignoreUnused(source);
				if (isSysexFromBCR2000(message)) {
					if (sysexCommand(message) == BCL_REPLY && message.getSysExDataSize() == 32) {
						int presetNum = message.getSysExData()[7] + 1;
						std::string presetName(&message.getSysExData()[8], &message.getSysExData()[32]);
						boost::trim(presetName);
						SimpleLogger::instance()->postMessage((boost::format("Preset #%d: %s") % presetNum % presetName).str());
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

	void BCR2000::invalidateListOfPresets()
	{
		bcrPresets_.clear();
	}

	MidiMessage BCR2000::requestEditBuffer() const
	{
		return requestDump(0x7F);
	}

	juce::MidiMessage BCR2000::requestDump(int number) const
	{
		std::vector<uint8> data = createSysexCommandData(REQUEST_DATA);
		data.push_back((uint8) number);
		return MidiHelpers::sysexMessage(data);
	}

	bool BCR2000::isPartOfDump(const MidiMessage& message) const
	{
		if (isSysexFromBCR2000(message)) {
			// This is the reply. We do not use the identity string returned
			if (message.getSysExDataSize() >= 5 && sysexCommand(message) == SEND_BCL_MESSAGE) {
				return true;
			}
		}
		return false;
	}

	bool BCR2000::isDumpFinished(std::vector<MidiMessage> const &bankDump) const
	{
		if (!bankDump.empty()) {
			std::string line = convertSyxToText(bankDump.back());
			if (line == "$end") {
				return true;
			}
		}
		return false;
	}

}
