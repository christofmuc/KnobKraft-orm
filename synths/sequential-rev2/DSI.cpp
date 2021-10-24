/*
   Copyright (c) 2019 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "DSI.h"

#include "MidiHelpers.h"

#include <boost/format.hpp>

namespace midikraft {

	std::map<int, std::string> kDSIAlternateTunings() {
		static std::map<int, std::string> sAlternateTunings = {
			{0, "12-Tone Equal Temperament"},
			{1, "Harmonic Series"},
			{2, "Carlos Harmonic Twelve Tone"},
			{3, "Meantone Temperament"},
			{4, "1/4 Tone Equal Temperament"},
			{5, "19 Tone Equal Temperament"},
			{6, "31 Tone Equal Temperament"},
			{7, "Pythagorean C"},
			{8, "Just Intonation in A with 7-limit Tritone at D#"},
			{9, "3-5 Lattice in A"},
			{10, "3-7 Lattice in A"},
			{11, "Other Music 7-Limit Black Keys in C"},
			{12, "Dan Schmidt Pelog/Slendro"},
			{13, "Yamaha Just Major C"},
			{14, "Yamaha Just Minor C"},
			{15, "Harry Partch 11-Limit 43 Just Intonation"},
			{16, "Arabic 12-Tone"},
		};
		return sAlternateTunings;
	}

	DSISynth::DSISynth(uint8 midiModelID) : midiModelID_(midiModelID), localControl_(true), midiControl_(true), updateSynthWithGlobalSettingsListener_(this)
	{
	}

	std::vector<juce::MidiMessage> DSISynth::deviceDetect(int channel)
	{
		ignoreUnused(channel);
		// This is identical for the OB-6 and the Rev2
		std::vector<uint8> sysEx({ 0b01111110, 0b01111111, 0b00000110 /* Inquiry Message */, 0b00000001 /* Inquiry Request */ });
		return { MidiMessage::createSysExMessage(&sysEx[0], (int)sysEx.size()) };
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

	std::vector<MidiMessage> DSISynth::requestEditBufferDump() const
	{
		return { MidiHelpers::sysexMessage({ 0b00000001, midiModelID_, 0b00000110 }) };
	}

	std::vector<juce::MidiMessage> DSISynth::requestPatch(int patchNo) const
	{
		uint8 bank = (uint8)((patchNo / numberOfPatches()) % numberOfBanks());
		uint8 program = (uint8) (patchNo % numberOfPatches());
		return std::vector<juce::MidiMessage>({ MidiHelpers::sysexMessage({ 0b00000001, midiModelID_, 0b00000101 /* Request program dump */, bank, program }) });
	}

	bool DSISynth::isEditBufferDump(const std::vector<MidiMessage>& message) const
	{
		// Again identical for Rev2 and OB-6
		return message.size() == 1 && isOwnSysex(message[0]) && message[0].getSysExDataSize() > 2 && message[0].getSysExData()[2] == 0x03;
	}

	bool DSISynth::isSingleProgramDump(const std::vector<MidiMessage>& message) const
	{
		// Again identical for Rev2 and OB-6
		return message.size() == 1 && isOwnSysex(message[0]) && message[0].getSysExDataSize() > 2 && message[0].getSysExData()[2] == 0x02;
	}

	MidiProgramNumber DSISynth::getProgramNumber(const std::vector<MidiMessage>&message) const
	{
		if (isSingleProgramDump(message)) {
			// Bank is stored in position 3, program number in position 4
			return MidiProgramNumber::fromZeroBase(message[0].getSysExData()[3] * numberOfPatches() + message[0].getSysExData()[4]);
		}
		return MidiProgramNumber::fromZeroBase(0);
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

	bool DSISynth::hasKeyboard() const
	{
		return true;
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

	std::vector<MidiMessage> DSISynth::createNRPN(int parameterNo, int value)
	{
		// Tried the first line to generate the NRPN in the same way the OB6 and Rev2 do it, but it does not make any difference in terms of fixing the sysex problems of the OB-6
		return MidiHelpers::generateRPN(channel().toOneBasedInt(), parameterNo, value, true, true, true);
		//return MidiRPNGenerator::generate(channel().toOneBasedInt(), parameterNo, value, true);
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
		// This is do work around a bug in the Rev2 firmware 1.1 that made the program edit buffer dump sent 3 bytes short, which is  bytes less after un-escaping
		while (result.size() < (size_t)expectedLength) {
			result.push_back(0);
		}
		return result;
	}

	std::vector<juce::uint8> DSISynth::escapeSysex(const PatchData &programEditBuffer, size_t bytesToEscape)
	{
		std::vector<juce::uint8> result;
		size_t readIndex = 0;
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

	std::vector<std::shared_ptr<TypedNamedValue>> DSISynth::getGlobalSettings()
	{
		return globalSettings_;
	}

	void DSISynth::setGlobalSettingsFromDataFile(std::shared_ptr<DataFile> dataFile)
	{
		if (dataFile && dataFile->dataTypeID() == settingsDataFileType()) {
			auto message = MidiMessage::createSysExMessage(dataFile->data().data(), (int)dataFile->data().size());
			std::vector<uint8> globalParameterData(&message.getSysExData()[3], message.getSysExData() + message.getSysExDataSize());
			// Loop over it and fill out the GlobalSettings Properties
			for (size_t i = 0; i < dsiGlobalSettings().size(); i++) {
				if (i < globalParameterData.size()) {
					// As this is coming from a datafile, we assume this is coming from the synth (we don't store the global settings data files on the computer)
					// Therefore, don't notify the update synth listener, because that would send out the same data back to the synth where it is coming from
					globalSettingsTree_.setPropertyExcludingListener(&updateSynthWithGlobalSettingsListener_,
						Identifier(dsiGlobalSettings()[i].typedNamedValue.name()),
						var(globalParameterData[dsiGlobalSettings()[i].sysexIndex] + dsiGlobalSettings()[i].displayOffset),
						nullptr);
				}
			}
		}
	}

	void DSISynth::GlobalSettingsListener::valueTreePropertyChanged(ValueTree& treeWhosePropertyHasChanged, const Identifier& property)
	{
		if (!synth_->wasDetected()) return;

		Value value = treeWhosePropertyHasChanged.getPropertyAsValue(property, nullptr, false);
		// Need to find definition for this setting now, suboptimal data structures
		for (const auto& def : synth_->dsiGlobalSettings()) {
			if (def.typedNamedValue.name() == property.getCharPointer()) {
				int newMidiValue = ((int)value.getValue()) - def.displayOffset;
				auto messages = synth_->createNRPN(def.nrpn, newMidiValue);
				String valueText;
				switch (def.typedNamedValue.valueType()) {
				case ValueType::Integer:
					valueText = String(int(value.getValue())); break;
				case ValueType::Bool:
					valueText = bool(value.getValue()) ? "On" : "Off"; break;
				case ValueType::Lookup:
					valueText = def.typedNamedValue.lookup()[int(value.getValue())]; break;
				default:
					//TODO not implemented yet
					jassert(false);
				}
				SimpleLogger::instance()->postMessage("Setting " + def.typedNamedValue.name() + " to " + valueText);				
				synth_->sendBlockOfMessagesToSynth(synth_->midiOutput(), messages);
				return;
			}
		}
	}

}


