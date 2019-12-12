/*
   Copyright (c) 2019 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "DSI.h"

#include "MidiHelpers.h"

#include <boost/format.hpp>

namespace midikraft {

	DSISynth::DSISynth(uint8 midiModelID) : midiModelID_(midiModelID), localControl_(true), midiControl_(true)
	{
	}

	juce::MidiMessage DSISynth::deviceDetect(int channel)
	{
		ignoreUnused(channel);
		// This is identical for the OB-6 and the Rev2
		std::vector<uint8> sysEx({ 0b01111110, 0b01111111, 0b00000110 /* Inquiry Message */, 0b00000001 /* Inquiry Request */ });
		return MidiMessage::createSysExMessage(&sysEx[0], (int)sysEx.size());
	}

	int DSISynth::deviceDetectSleepMS()
	{
		// Haven't tried, just assume standard turnaround
		return 100;
	}

	bool DSISynth::isOwnSysex(MidiMessage const &message) const
	{
		return message.isSysEx()
			&& message.getSysExDataSize() > 1
			&& message.getSysExData()[0] == 0x01  /* DSI = Sequential */
			&& message.getSysExData()[1] == midiModelID_;
	}

	MidiChannel DSISynth::channelIfValidDeviceResponse(const MidiMessage &message)
	{
		if (message.isSysEx() && message.getSysExDataSize() > 9) {
			auto data = message.getSysExData();
			bool isSynthWeLookFor = data[0] == 0b01111110
				&& data[2] == 0b00000110 // Inquiry message
				&& data[3] == 0b00000010 // Inquiry Reply 
				&& data[4] == 0x01; // DSI

			if (!isSynthWeLookFor)
				return MidiChannel::invalidChannel();

			if (data[5] != midiModelID_)
				return MidiChannel::invalidChannel();

			// Found one!
			//int familyMS = data[6];
			//int familiyMemberLS = data[7];
			//int familiyMemberMS = data[8];
			int versionMajor = data[9]; // This is different from the Rev2 manual, which states that the version is within one byte
			int versionMinor = data[10];
			int versionPatch = data[11];
			versionString_ = (boost::format("%d.%d.%d") % versionMajor % versionMinor % versionPatch).str();
			if (data[1] == 0b01111111) {
				//Omni seems to be 0b01111111 at DSI
				return MidiChannel::omniChannel();
			}
			return MidiChannel::fromZeroBase(data[1]); // MIDI Channel of reply 
		}
		return MidiChannel::invalidChannel();
	}

	bool DSISynth::needsChannelSpecificDetection()
	{
		return false;
	}

	MidiMessage DSISynth::requestEditBufferDump()
	{
		return MidiHelpers::sysexMessage({ 0b00000001, midiModelID_, 0b00000110 });
	}

	std::vector<juce::MidiMessage> DSISynth::requestPatch(int patchNo)
	{
		uint8 bank = (uint8)(patchNo / 100);
		uint8 program = patchNo % 100;
		return std::vector<juce::MidiMessage>({ MidiHelpers::sysexMessage({ 0b00000001, midiModelID_, 0b00000101 /* Request program dump */, bank, program }) });
	}

	bool DSISynth::isEditBufferDump(const MidiMessage& message) const
	{
		// Again identical for Rev2 and OB-6
		return isOwnSysex(message) && message.getSysExDataSize() > 2 && message.getSysExData()[2] == 0x03;
	}

	bool DSISynth::isSingleProgramDump(const MidiMessage& message) const
	{
		// Again identical for Rev2 and OB-6
		return isOwnSysex(message) && message.getSysExDataSize() > 2 && message.getSysExData()[2] == 0x02;
	}

	MidiChannel DSISynth::getOutputChannel() const
	{
		// There is no difference between the output and the input channel
		return channel();
	}

	bool DSISynth::hasLocalControl() const
	{
		return true;
	}

	bool DSISynth::getLocalControl() const
	{
		return localControl_;
	}

	bool DSISynth::canChangeInputChannel() const
	{
		return true;
	}

	MidiChannel DSISynth::getInputChannel() const
	{
		// There is no difference between the output and the input channel
		return channel();
	}

	bool DSISynth::hasMidiControl() const
	{
		return true;
	}

	bool DSISynth::isMidiControlOn() const
	{
		return midiControl_;
	}

	juce::MidiMessage DSISynth::saveEditBufferToProgram(int programNumber)
	{
		ignoreUnused(programNumber);
		// That's not supported, AFAIK
		return MidiMessage();
	}

	juce::MidiBuffer DSISynth::createNRPN(int parameterNo, int value)
	{
		return MidiRPNGenerator::generate(channel().toOneBasedInt(), parameterNo, value, true);
	}

	Synth::PatchData DSISynth::unescapeSysex(const uint8 *sysExData, int sysExLen, int expectedLength)
	{
		PatchData result;
		int dataIndex = 0;
		while (dataIndex < sysExLen) {
			uint8 ms_bits = sysExData[dataIndex];
			dataIndex++;
			for (int i = 0; i < 7; i++) {
				// Actually, the last 7 byte block might be incomplete, as the original number of data bytes might not be a
				// multitude of 7. Instead of buffering with 0, the DSI folks terminate the block with less than 7 bytes
				if (dataIndex < sysExLen) {
					result.push_back(sysExData[dataIndex] | ((ms_bits & (1 << i)) << (7 - i)));
				}
				dataIndex++;
			}
		}
		// This is do work around a bug in the Rev2 firmware 1.1 that made the program edit buffer dump sent 3 bytes short, which is  bytes less after unescaping
		while (result.size() < expectedLength) {
			result.push_back(0);
		}
		return result;
	}

	std::vector<juce::uint8> DSISynth::escapeSysex(const PatchData &programEditBuffer, size_t bytesToEscape)
	{
		std::vector<juce::uint8> result;
		int readIndex = 0;
		while (readIndex < bytesToEscape) {
			// Write an empty msb byte and record the index
			result.push_back(0);
			size_t msbIndex = result.size() - 1;

			// Now append the next 7 bytes from the input, and set their msb byte
			uint8 msb = 0;
			for (int i = 0; i < 7; i++) {
				if (readIndex < bytesToEscape) {
					result.push_back(programEditBuffer.at(readIndex) & 0x7f);
					msb |= (programEditBuffer.at(readIndex) & 0x80) >> (7 - i);
				}
				readIndex++;
			}

			// Then poke the msb back into the result stream
			result.at(msbIndex) = msb;
		}
		return result;
	}

}
