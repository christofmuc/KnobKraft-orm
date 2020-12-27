/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "RefaceDX.h"

#include "RefaceDXPatch.h"

#include "MidiHelpers.h"
#include "Sysex.h"
#include "MidiController.h"

#include <boost/format.hpp>

namespace midikraft {

	std::vector<Range<int>> kRefaceDXBlankOutZones = {
		{ 0, 10 }, // 10 Characters for the name, the first bytes of the common block so it's really at the start of the data
	};


	juce::MidiMessage RefaceDX::buildRequest(uint8 addressHigh, uint8 addressMid, uint8 addressLow) const
	{
		return MidiHelpers::sysexMessage(
			{ 0x43 /* Yamaha */, (uint8)(0x20 /* dump request */ | deviceID_), 0x7f /* Group high */, 0x1c /* Group low */, 0x05 /* Model */, addressHigh, addressMid, addressLow }
		);
	}

	juce::MidiMessage RefaceDX::buildParameterChange(uint8 addressHigh, uint8 addressMid, uint8 addressLow, uint8 value)
	{
		return MidiHelpers::sysexMessage(
			{ 0x43 /* Yamaha */, (uint8)(0x10 /* parameter change */ | deviceID_), 0x7f /* Group high */, 0x1c /* Group low */, 0x05 /* Model */, addressHigh, addressMid, addressLow, value }
		);
	}

	std::vector<MidiMessage> RefaceDX::requestStreamElement(int elemNo, StreamType streamType) const
	{
		switch (streamType) {
		case StreamLoadCapability::StreamType::BANK_DUMP:
			// Need to start at program 0
			return { MidiMessage::programChange(channel().toOneBasedInt(), elemNo), buildRequest(0x0e, 0x0f, 0x00 /* Address of bulk header */) };
		case StreamLoadCapability::StreamType::EDIT_BUFFER_DUMP:
			// Just download the current program
			return { buildRequest(0x0e, 0x0f, 0x00 /* Address of bulk header */) };
		default:
			jassertfalse;
			return {};
		}
	}

	int RefaceDX::numberOfStreamMessagesExpected(StreamType streamType) const
	{
		switch (streamType)
		{
		case StreamLoadCapability::StreamType::EDIT_BUFFER_DUMP:
			return 7;
		case StreamLoadCapability::StreamType::BANK_DUMP:
			return 32 + 32 + 32 + 32 * 4;
		default:
			return 0;
		}
	}

	bool RefaceDX::isMessagePartOfStream(MidiMessage const &message, StreamType streamType) const
	{
		ignoreUnused(streamType); // Both stream types consist of the same message types
		if (isOwnSysex(message)) {
			TDataBlock block;
			if (dataBlockFromDump(message, block)) {
				return (block.addressHigh == 0x0e && block.addressMid == 0x0f && block.addressLow == 0x00)  // Bulk header
					|| (block.addressHigh == 0x0f && block.addressMid == 0x0f && block.addressLow == 0x00)  // Bulk footer
					|| (block.addressHigh == 0x30 && block.addressMid == 0x00 && block.addressLow == 0x00)  // Common voice data
					|| (block.addressHigh == 0x31 && block.addressLow == 0x00); // Operator data
			}
		}
		return false;
	}

	bool RefaceDX::isStreamComplete(std::vector<MidiMessage> const &messages, StreamType streamType) const {
		int headers, footers, common, operators;
		headers = footers = common = operators = 0;
		for (auto message : messages) {
			TDataBlock block;
			if (dataBlockFromDump(message, block)) {
				if (block.addressHigh == 0x0e && block.addressMid == 0x0f && block.addressLow == 0x00) headers++;
				if (block.addressHigh == 0x0f && block.addressMid == 0x0f && block.addressLow == 0x00)  footers++;
				if (block.addressHigh == 0x30 && block.addressMid == 0x00 && block.addressLow == 0x00)  common++;
				if (block.addressHigh == 0x31 && block.addressLow == 0x00) operators++;
			}
			else {
				jassert(false);
			}
		}
		switch (streamType) {
		case StreamLoadCapability::StreamType::BANK_DUMP:
			return headers == 32 && footers == 32 && common == 32 && operators == (32 * 4);
		case StreamLoadCapability::StreamType::EDIT_BUFFER_DUMP:
			return headers == 1 && footers == 1 && common == 1 && operators == (1 * 4);
		default:
			jassertfalse;
			return false;
		}
	}

	bool RefaceDX::shouldStreamAdvance(std::vector<MidiMessage> const &messages, StreamType streamType) const
	{
		int headers, footers;
		headers = footers = 0;
		for (auto message : messages) {
			TDataBlock block;
			if (dataBlockFromDump(message, block)) {
				if (block.addressHigh == 0x0e && block.addressMid == 0x0f && block.addressLow == 0x00) headers++;
				if (block.addressHigh == 0x0f && block.addressMid == 0x0f && block.addressLow == 0x00)  footers++;
			}
			else {
				jassert(false);
			}
		}
		return (headers == footers) && (streamType == StreamLoadCapability::StreamType::BANK_DUMP); // The Edit buffer dump would be finished, and does not need to advance
	}

	void RefaceDX::changeOutputChannel(MidiController *controller, MidiChannel newChannel, std::function<void()> finished)
	{
		// Transmit Channel has the address 0, 0, 0
		controller->getMidiOutput(midiOutput())->sendMessageNow(
			buildParameterChange(0, 0, 0, newChannel.isValid() ? ((uint8)newChannel.toZeroBasedInt()) : 0x7f)
		);
		transmitChannel_ = newChannel;
		finished();
	}

	MidiChannel RefaceDX::getOutputChannel() const
	{
		return transmitChannel_;
	}

	bool RefaceDX::hasLocalControl() const
	{
		return true;
	}

	void RefaceDX::setLocalControl(MidiController *controller, bool localControlOn)
	{
		// Local control has the address 0, 0, 6
		controller->getMidiOutput(midiOutput())->sendMessageNow(buildParameterChange(0, 0, 6, localControlOn ? 1 : 0));
		localControl_ = localControlOn;
	}

	bool RefaceDX::getLocalControl() const
	{
		return localControl_;
	}

	bool RefaceDX::canChangeInputChannel() const
	{
		return true;
	}

	void RefaceDX::changeInputChannel(MidiController *controller, MidiChannel channel, std::function<void()> finished)
	{
		// Receive channel has the address 0, 0, 1
		controller->getMidiOutput(midiOutput())->sendMessageNow(
			buildParameterChange(0, 0, 1, channel.isValid() ? ((uint8)channel.toZeroBasedInt()) : 0x10 /* OMNI. We could dispute if we should rather set MIDI Control to Off */)
		);
		setCurrentChannelZeroBased(midiInput(), midiOutput(), channel.toZeroBasedInt());
		finished();
	}

	MidiChannel RefaceDX::getInputChannel() const
	{
		return channel();
	}

	bool RefaceDX::hasMidiControl() const
	{
		return true;
	}

	bool RefaceDX::isMidiControlOn() const
	{
		return midiControlOn_;
	}

	void RefaceDX::setMidiControl(MidiController *controller, bool isOn)
	{
		controller->getMidiOutput(midiOutput())->sendMessageNow(
			buildParameterChange(0, 0, 0x0e, isOn ? 1 : 0)
		);
		midiControlOn_ = isOn;
	}

	bool RefaceDX::dataBlockFromDump(const MidiMessage &message, TDataBlock &outBlock) const {
		if (isOwnSysex(message)) {
			// Check the number of bytes 
			uint16 dataLength = ((uint16)message.getSysExData()[4]) << 7 | message.getSysExData()[5];
			if (message.getSysExDataSize() == dataLength + 7) {
				// Looks good up to here, now extract the data block, but don't copy the checksum (hence - 1)
				outBlock.data.clear();
				int sum = 0;
				for (int i = 0; i < dataLength - 1; i++) {
					uint8 dataByte = message.getSysExData()[7 + i];
					switch (i) {
					case 0: outBlock.addressHigh = dataByte; break;
					case 1: outBlock.addressMid = dataByte; break;
					case 2: outBlock.addressLow = dataByte; break;
					default:
						outBlock.data.push_back(dataByte);
					}
					sum -= dataByte;
				}
				// Strangely, the Model ID 0x05 is included in the checksum. Manually add in back in here, as we did not get it out
				uint8 checkSum = ((sum - 0x05) & 0x7f);
				uint8 expected = message.getSysExData()[7 + dataLength - 1];
				if (checkSum == expected) {
					return true;
				}
				else {
					// Checksum error
					return false;
				}
			}
		}
		return false;
	}

	MidiMessage RefaceDX::buildDataBlockMessage(TDataBlock const &block) const {
		std::vector<uint8> bulkDump(
			{ 0x43 /* Yamaha */, (uint8)(0x00 /* bulk dump */ | deviceID_), 0x7f /* Group high */, 0x1c /* Group low */, 0, 0, 0x05 /* Model */,
				block.addressHigh, block.addressMid, block.addressLow }
		);
		std::copy(block.data.begin(), block.data.end(), std::back_inserter(bulkDump));

		// We need a checksum
		int sum = 0;
		std::for_each(bulkDump.begin() + 6, bulkDump.end(), [&](uint8 byte) { sum -= byte; });
		bulkDump.push_back(sum & 0x7f);

		// We need to record the number of bytes in this message
		uint16 dataBytes = ((uint16)bulkDump.size()) - 7;
		bulkDump[4] = (uint8)(dataBytes >> 7);
		bulkDump[5] = dataBytes & 0x7f;

		return MidiHelpers::sysexMessage(bulkDump);
	}

	std::string RefaceDX::getName() const
	{
		return "Yamaha Reface DX";
	}

	bool RefaceDX::isOwnSysex(MidiMessage const &message) const
	{
		if (message.isSysEx()) {
			if (message.getSysExDataSize() > 6) {
				// Strangely asymmetric with the message we send to the Reface
				return message.getSysExData()[0] == 0x43 /* Yamaha */
					&& message.getSysExData()[2] == 0x7f /* Group high */
					&& message.getSysExData()[3] == 0x1c /* Group low */
					&& message.getSysExData()[6] == 0x05 /* Model */;
			}
		}
		return false;
	}

	Synth::PatchData RefaceDX::filterVoiceRelevantData(std::shared_ptr<DataFile> unfilteredData) const
	{
		return Patch::blankOut(kRefaceDXBlankOutZones, unfilteredData->data());
	}

	std::shared_ptr<DataFile> RefaceDX::patchFromPatchData(const Synth::PatchData &data, MidiProgramNumber place) const
	{
		auto newPatch = std::make_shared<RefaceDXPatch>(data, place);
		return newPatch;
	}

	std::vector<juce::MidiMessage> RefaceDX::patchToSysex(std::shared_ptr<DataFile> patch) const
	{
		std::vector<juce::MidiMessage> result;

		result.push_back(buildDataBlockMessage(TDataBlock(0x0e, 0x0f, 0x00, std::vector<uint8>().begin(), 0)));
		result.push_back(buildDataBlockMessage(TDataBlock(0x30, 0x00, 0x00, patch->data().begin(), 38)));
		for (size_t op = 0; op < 4; op++) {
			result.push_back(buildDataBlockMessage(TDataBlock(0x31, (uint8)op, 0x00, patch->data().begin() + 38 + op * 28, 28)));
		}
		result.push_back(buildDataBlockMessage(TDataBlock(0x0f, 0x0f, 0x00, std::vector<uint8>().begin(), 0)));
		return result;
	}

	std::vector<juce::MidiMessage> RefaceDX::deviceDetect(int channel)
	{
		ignoreUnused(channel);
		// Instead of using the official device ID request, we will just send a dump request for the system settings, in which
		// we will find both receiving and sending channel (which, excellently, can be different on the Reface).
		return { buildRequest(0x00, 0x00, 0x00 /* Address system settings */) };
	}

	MidiChannel RefaceDX::channelIfValidDeviceResponse(const MidiMessage &message)
	{
		if (message.isSysEx()) {
			TDataBlock block;
			if (dataBlockFromDump(message, block)) {
				// We expect the system data dump here
				if ((block.data.size() == 32) && block.addressHigh == 0x00 && block.addressMid == 0x00 && block.addressLow == 0x00) {
					uint8 transmitChannel = block.data[0]; // Transmit channel of Reface DX
					if (transmitChannel != 0x7f) {
						// It is not off
						transmitChannel_ = MidiChannel::fromZeroBase(transmitChannel);
					}
					localControl_ = block.data[6] == 1;
					midiControlOn_ = block.data[0x0e] == 1;
					uint8 midiChannel = block.data[1]; // Receiving channel of Reface DX
					if (midiChannel == 0x10) {
						// That's omni, not good for your setup, so let's log a warning
						SimpleLogger::instance()->postMessage("Warning: Your RefaceDX is set to receive MIDI omni, so it will react on all channels");
						return MidiChannel::omniChannel();
					}
					return MidiChannel::fromZeroBase(midiChannel);
				}
			}
		}
		return MidiChannel::invalidChannel();
	}

	TPatchVector RefaceDX::loadPatchesFromStream(std::vector<MidiMessage> const &sysexMessages) const 
	{
		// Patches loaded from a list of MidiMessages... First we need to find complete "voices"
		std::vector<RefaceDXPatch::TVoiceData> voiceData;
		bool patchActive = false;
		int count = 0;
		for (const auto& message : sysexMessages) {
			TDataBlock block;
			if (dataBlockFromDump(message, block)) {
				if (block.addressHigh == 0x0e && block.addressMid == 0x0f && block.addressLow == 0x00) {
					// Bulk header
					if (patchActive) {
						Logger::getCurrentLogger()->writeToLog("Warning: Parsing RefaceDX sysex - got bulk header before footer of previous bulk block, ignored");
					}
					else {
						// Create a new but empty voice data entry
						voiceData.push_back(RefaceDXPatch::TVoiceData());
						voiceData.back().count = count++;
					}
					patchActive = true;
				}
				else if (block.addressHigh == 0x0f && block.addressMid == 0x0f && block.addressLow == 0x00) {
					// Bulk footer
					if (!patchActive) {
						Logger::getCurrentLogger()->writeToLog("Warning: Parsing RefaceDX sysex - got bulk footer before header of bulk block, ignored");
					}
					patchActive = false;
				}
				else if (patchActive) {
					if (block.addressHigh == 0x30 && block.addressMid == 0x00 && block.addressLow == 0x00) {
						// Common Voice data
						if (voiceData.back().common.size() == 0) {
							voiceData.back().common = block.data;
						}
						else {
							Logger::getCurrentLogger()->writeToLog("Warning: Parsing RefaceDX sysex - got second common voice block within bulk block, ignored");
						}
					}
					else if (block.addressHigh == 0x31 && block.addressLow == 0x00) {
						uint8 operatorIndex = block.addressMid;
						if (operatorIndex >= 0 && operatorIndex < 4) {
							if (voiceData.back().op[operatorIndex].size() == 0) {
								voiceData.back().op[operatorIndex] = block.data;
							}
							else {
								Logger::getCurrentLogger()->writeToLog("Warning: Parsing RefaceDX sysex - got additional operator block within bulk block, ignored");
							}
						}
					}
					else {
						Logger::getCurrentLogger()->writeToLog("Warning: Parsing RefaceDX sysex - got invalid block within bulk block, ignored");
					}
				}
				else {
					//TODO - this could be a system block
					Logger::getCurrentLogger()->writeToLog("Warning: Parsing RefaceDX sysex - got block outside of bulk block, ignored");
				}
			}
		}

		// We now might have or not a list of valid VoiceData packages, which we can wrap into patch classes
		TPatchVector result;
		for (auto voice : voiceData) {
			std::vector<uint8> aggregated;
			std::copy(voice.common.begin(), voice.common.end(), std::back_inserter(aggregated));
			for (int op = 0; op < 4; op++) std::copy(voice.op[op].begin(), voice.op[op].end(), std::back_inserter(aggregated));
			result.push_back(std::make_shared<RefaceDXPatch>(aggregated, MidiProgramNumber::fromZeroBase(voice.count)));
		}
		return result;
	}

	std::vector<juce::MidiMessage> RefaceDX::dataFileToMessages(std::shared_ptr<DataFile> dataFile, std::shared_ptr<SendTarget> target) const
	{
		ignoreUnused(target);
		return patchToSysex(dataFile);
	}

	int RefaceDX::numberOfBanks() const
	{
		// Yes, I know, it has 4 banks of 8 patches each. But I refuse.
		return 1;
	}

	int RefaceDX::numberOfPatches() const
	{
		return 32;
	}

	std::string RefaceDX::friendlyProgramName(MidiProgramNumber programNo) const
	{
		int bank = programNo.toZeroBased() / 8;
		int patch = programNo.toZeroBased() % 8;
		return (boost::format("Bank%d-%d") % bank % patch).str();
	}

	std::string RefaceDX::friendlyBankName(MidiBankNumber bankNo) const
	{
		ignoreUnused(bankNo);
		return "Banks 1-4";
	}

}
