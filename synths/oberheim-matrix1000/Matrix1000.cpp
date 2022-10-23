/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "Matrix1000.h"

#include "SynthParameterDefinition.h"
#include "Synth.h"

#include <boost/format.hpp>

#include "Matrix1000Patch.h"
#include "Matrix1000_GlobalSettings.h"

//#include "Matrix1000BCR.h"
//#include "BCR2000.h"

#include "MidiHelpers.h"

#include <set>
//#include "BCR2000_Presets.h"

namespace midikraft {

	const struct {
		uint8 OBERHEIM = 0x10;
		uint8 MATRIX6_1000 = 0x06;
		uint8 MATRIX1000_VERSION = 0x03; // To differentiate the master data from that of a Matrix6, I guess
	} MIDI_ID;

	const struct {
		uint8 SINGLE_PATCH_DATA = 0x01;
		uint8 REQUEST_DATA = 0x04;
		uint8 SET_BANK = 0x0a;
		uint8 PARAMETER_EDIT = 0x0b;
		uint8 BANK_UNLOCK = 0x0c;
		uint8 SINGLE_PATCH_TO_EDIT_BUFFER = 0x0d;
		uint8 STORE_EDIT_BUFFER = 0x0e;
	} MIDI_COMMAND;

	// Definition for the "unused" data bytes inside the sysex of the Matrix 1000
	// These unused bytes need to be blanked for us to compare patches in order to detect duplicates
	std::vector<Range<size_t>> kMatrix1000BlankOutZones = {
		{0, 8} // This is the ASCII name, 8 character. The Matrix1000 will never display it, but I think a Matrix6 will
	};

	struct Matrix1000GlobalSettingDefinition {
		int sysexIndex;
		TypedNamedValue typedNamedValue;
		bool isTwosComplement;
		int displayOffset;
	};

	// Table of global settings. What I left out was the "group enabled" array for one bit per patch to specify if it is deemed to be group-worthy.
	struct Matrix1000GlobalSettings {
		std::vector<Matrix1000GlobalSettingDefinition> definitions = {
			{ 34, { "Master Transpose", "Tuning", 0, -24, 24 }, true },
			{ 8, { "Master Tune", "Tuning", 0, -32, 32 }, true },
			{ 11, { "MIDI Basic Channel", "MIDI", 1, 1, 16 }, false, 1 /* Make it one based */},
			{ 12, { "MIDI OMNI Mode Enable", "MIDI", false } },
			{ 13, { "MIDI Controllers enable", "MIDI", true} },
			{ 14, { "MIDI Patch Changes Enable", "MIDI", true } },
			{ 17, { "MIDI Pedal 1 Controller", "MIDI", 0, 0, 121 } }, //TODO - could make a lookup of CC controller names? 121 is from the manual
			{ 18, { "MIDI Pedal 2 Controller", "MIDI", 0, 0, 121 } },
			{ 19, { "MIDI Pedal 3 Controller", "MIDI", 0, 0, 121 } },
			{ 20, { "MIDI Pedal 4 Controller", "MIDI", 0, 0, 121 } },
			{ 32, { "MIDI Echo Enable", "MIDI", false } },
			{ 35, { "MIDI Mono Mode (Guitar)", "MIDI", false } },
			{ 165, { "Bank Lock Enable", "MIDI", false} }, // (In MSB only)
			{ 4, { "Vibrato Waveform", "Global Vibrato", 0, { {0, "Triangle" }, { 1, "Saw up" }, { 2, "Saw Down" }, { 3, "Square" }, { 4, "Random" }, { 5, "Noise" } } } },
			{ 1, { "Vibrato Speed", "Global Vibrato", 0, 0, 63 } },
			{ 5, { "Vibrato Amplitude", "Global Vibrato", 0, 0, 63 } },
			{ 2, { "Vibrato Speed Mod Source", "Global Vibrato", 0, { {0, "Off" }, { 1, "Lever 2" }, { 2, "Pedal 1" } } } },
			{ 3, { "Vibrato Speed Mod Amount", "Global Vibrato", 0, 0, 63 } },
			{ 6, { "Vibrato Amp Mod Source", "Global Vibrato", 0, { {0, "Off" }, { 1, "Lever 2" }, { 2, "Pedal 1" } } } },
			{ 7, { "Vibrato Amp Mod Amount", "Global Vibrato", 0, 0, 63 } },
			{ 164, { "Bend Range", "Controls", 2, 1, 24 } },
			{ 166, { "Number of Units", "Group Mode", 1, 1, 6 } },
			{ 167, { "Current Unit Number", "Group Mode", 0, 0, 7 } }, // (In MSB only)
			{ 168, { "Group Mode Enable", "Group Mode", false } }, // (In MSB only)
			{ 169, { "Unison Enable", "General", false } },
			{ 170, { "Volume Invert Enable", "General", false } },
			{ 171, { "Memory Protect Enable", "General", false } },
		};
	};
	std::shared_ptr<Matrix1000GlobalSettings> gMatrix1000GlobalSettings;
	std::shared_ptr<Matrix1000GlobalSettings> sMatrix1000GlobalSettings() {
		if (!gMatrix1000GlobalSettings) {
			gMatrix1000GlobalSettings = std::make_shared<Matrix1000GlobalSettings>();
		}
		return gMatrix1000GlobalSettings;
	}

	std::vector<MidiMessage> Matrix1000::requestEditBufferDump() const
	{
		return { createRequest(EDIT_BUFFER, 0x00) };
	}

	MidiMessage Matrix1000::createRequest(REQUEST_TYPE typeNo, uint8 number) const
	{
		/* 04H - Request Data

		F0H 10H 06H 04H <type> <number> F7H

		<type>     = 0 to request all patches in current bank and master parameters.
				   = 1 to request a single patch from the current bank
				   = 3 to request master parameters
				   = 4 to request edit buffer
		<number>   = 0 when <type> = 0 or 3
				   = Number of patch requested when <type> = 1
				   */
		return MidiHelpers::sysexMessage({ MIDI_ID.OBERHEIM, MIDI_ID.MATRIX6_1000, MIDI_COMMAND.REQUEST_DATA, (uint8)typeNo, (uint8)((typeNo == REQUEST_TYPE::SINGLE_PATCH) ? number : 0) });
	}

	MidiMessage Matrix1000::createBankSelect(MidiBankNumber bankNo) const {
		if (!bankNo.isValid()) {
			jassert(false);
			return MidiMessage();
		}
		return MidiHelpers::sysexMessage({ MIDI_ID.OBERHEIM, MIDI_ID.MATRIX6_1000, MIDI_COMMAND.SET_BANK, (uint8)bankNo.toZeroBased() });
	}

	MidiMessage Matrix1000::createBankUnlock() const {
		return MidiHelpers::sysexMessage({ MIDI_ID.OBERHEIM, MIDI_ID.MATRIX6_1000, MIDI_COMMAND.BANK_UNLOCK });
	}

	void Matrix1000::initGlobalSettings()
	{
		// Loop over it and fill out the GlobalSettings Properties
		globalSettings_.clear();
		for (auto & matrix1000GlobalSetting : sMatrix1000GlobalSettings()->definitions) {
			auto setting = std::make_shared<TypedNamedValue>(matrix1000GlobalSetting.typedNamedValue);
			globalSettings_.push_back(setting);
		}
		globalSettingsTree_ = ValueTree("M1000SETTINGS");
		globalSettings_.addToValueTree(globalSettingsTree_);
		globalSettingsTree_.addListener(&updateSynthWithGlobalSettingsListener_);
	}

	void Matrix1000::GlobalSettingsListener::valueTreePropertyChanged(ValueTree& treeWhosePropertyHasChanged, const Identifier& property)
	{
		// No matter which value changed, we need to create a new global settings sysex message and queue it for sending debounced
		ignoreUnused(treeWhosePropertyHasChanged, property);

		if (!globalSettingsData_.empty() && synth_->wasDetected()) {
			auto newMessage = globalSettingsData_;
			// Poke all values from the globalSettings array into the data
			for (auto & setting : synth_->globalSettings_) {
				//TODO Need to find definition for this setting now, suboptimal data structures
				for (auto const &def : sMatrix1000GlobalSettings()->definitions) {
					if (def.typedNamedValue.name() == setting->name()) {
						int newMidiValue = ((int)setting->value().getValue()) - def.displayOffset;
						if (def.isTwosComplement) {
							if (newMidiValue < 0) {
								newMidiValue = (uint8)newMidiValue;
							}
						}
						newMessage[def.sysexIndex] = (uint8)newMidiValue;
					}
				}
			}

			// Done, now create a new global sysex dump message
			std::vector<uint8> globalSettingsData({ MIDI_ID.OBERHEIM, MIDI_ID.MATRIX6_1000, REQUEST_TYPE::MASTER, MIDI_ID.MATRIX1000_VERSION });
			auto patchdata = synth_->escapeSysex(newMessage);
			std::copy(patchdata.begin(), patchdata.end(), std::back_inserter(globalSettingsData));
			MidiController::instance()->getMidiOutput(synth_->midiOutput())->sendMessageDebounced(MidiHelpers::sysexMessage(globalSettingsData), 800);
		}
	}

	bool Matrix1000::isOwnSysex(MidiMessage const &message) const
	{
		return MidiHelpers::isSysexMessageMatching(message, { {0, MIDI_ID.OBERHEIM}, { 1, MIDI_ID.MATRIX6_1000 } });
	}

	int Matrix1000::numberOfBanks() const
	{
		return 10;
	}

	int Matrix1000::numberOfPatches() const
	{
		return 100;
	}

	std::string Matrix1000::friendlyProgramName(MidiProgramNumber programNo) const
	{
		// The Matrix does a 3 digit display, with the first patch being "000" and the highest being "999"s 
		return (boost::format("%03d") % programNo.toZeroBased()).str();
	}

	std::string Matrix1000::friendlyBankName(MidiBankNumber bankNo) const
	{
		return (boost::format("%03d - %03d") % (bankNo.toZeroBased() * numberOfPatches()) % (bankNo.toOneBased() * numberOfPatches() - 1)).str();
	}

	bool Matrix1000::isEditBufferDump(const std::vector<MidiMessage>& message) const
	{
		// The Matrix1000 either sends Edit Buffers as Program Dumps, or it is a Single Patch Data to Edit Buffer message, which the M1k will never generate on its own, 
		// but we will when we save data to disk.
		return isSingleProgramDump(message) || 
			(message.size() == 1 && isOwnSysex(message[0]) && MidiHelpers::isSysexMessageMatching(message[0], { {2, MIDI_COMMAND.SINGLE_PATCH_TO_EDIT_BUFFER}, { 3, (uint8)0x00} }));
	}


	bool Matrix1000::isSingleProgramDump(const std::vector<MidiMessage>& message) const
	{
		return message.size() == 1 && isOwnSysex(message[0])
			&& message[0].getSysExDataSize() > 3
			&& message[0].getSysExData()[2] == MIDI_COMMAND.SINGLE_PATCH_DATA
			&& message[0].getSysExData()[3] >= 0x00
			&& message[0].getSysExData()[3] < 100; // Should be a valid program number in this bank
	}

	MidiProgramNumber Matrix1000::getProgramNumber(const std::vector<MidiMessage>&message) const
	{
		if (isSingleProgramDump(message)) {
			return MidiProgramNumber::fromZeroBase(message[0].getSysExData()[3]);
		}
		return MidiProgramNumber::fromZeroBase(0);
	}

	std::shared_ptr<DataFile> Matrix1000::patchFromProgramDumpSysex(const std::vector<MidiMessage>& message) const
	{
		if (isSingleProgramDump(message)) {
			//TODO doesn't check length of data provided
			auto matrixPatch = std::make_shared<Matrix1000Patch>(unescapeSysex(&message[0].getSysExData()[4], message[0].getSysExDataSize() - 4), getProgramNumber(message));
			return matrixPatch;
		}
		return nullptr;
	}

	std::vector<juce::MidiMessage> Matrix1000::patchToProgramDumpSysex(std::shared_ptr<DataFile> patch, MidiProgramNumber programNumber) const
	{
		uint8 programNo = programNumber.toZeroBased() % 100;
		std::vector<uint8> singleProgramDump({ MIDI_ID.OBERHEIM, MIDI_ID.MATRIX6_1000, MIDI_COMMAND.SINGLE_PATCH_DATA, programNo });
		auto patchdata = escapeSysex(patch->data());
		std::copy(patchdata.begin(), patchdata.end(), std::back_inserter(singleProgramDump));
		return std::vector<MidiMessage>({ MidiHelpers::sysexMessage(singleProgramDump) });
	}

	std::vector<MidiMessage> Matrix1000::requestStreamElement(int no, StreamType streamType) const
	{
		switch (streamType) {
		case StreamLoadCapability::StreamType::BANK_DUMP: {
			MidiBankNumber bankNo = MidiBankNumber::fromZeroBase(no, 100);
			if (!bankNo.isValid()) {
				return {};
			}
			return { createBankSelect(bankNo), createRequest(BANK_AND_MASTER, 0) };
		}
		case StreamLoadCapability::StreamType::EDIT_BUFFER_DUMP:
			return { requestEditBufferDump() };
		default:
			SimpleLogger::instance()->postMessage("The Matrix1000 does not support loading this stream type");
			return {};
		}
	}

	int Matrix1000::numberOfStreamMessagesExpected(StreamType streamType) const
	{
		switch (streamType) {
		case StreamLoadCapability::StreamType::BANK_DUMP:
			return 100 + 0x50 + 1;
		case StreamLoadCapability::StreamType::EDIT_BUFFER_DUMP:
			return 1;
		default:
			return 0;
		}
	}

	bool Matrix1000::isMessagePartOfStream(const MidiMessage& message, StreamType streamType) const
	{
		switch (streamType) {
		case StreamLoadCapability::StreamType::BANK_DUMP:
			return isSingleProgramDump({ message }) || isSplitPatch(message) || globalSettingsLoader_->isDataFile(message, 0);
		case StreamLoadCapability::StreamType::EDIT_BUFFER_DUMP:
			return isEditBufferDump({ message });
		default:
			return false;
		}
	}

	bool Matrix1000::isStreamComplete(std::vector<MidiMessage> const &messages, StreamType streamType) const
	{
		// Count the number of patch dumps in that stream
		int found = 0;
		int split = 0;
		int master = 0;
		int editbuffer = 0;
		for (auto message : messages) {
			switch (streamType) {
			case midikraft::StreamLoadCapability::StreamType::BANK_DUMP:
				if (isSingleProgramDump({ message })) {
					found++;
				}
				else if (isSplitPatch(message)) {
					split++;
				}
				else if (globalSettingsLoader_->isDataFile(message, 0 /* TODO this is ignored */)) {
					master++;
				}
				break;
			case midikraft::StreamLoadCapability::StreamType::EDIT_BUFFER_DUMP:
				if (isEditBufferDump({ message })) {
					editbuffer++;
				}
				break;
			default:
				break;
			}
		}
		switch (streamType)
		{
		case midikraft::StreamLoadCapability::StreamType::EDIT_BUFFER_DUMP:			
			return editbuffer > 0;
		case midikraft::StreamLoadCapability::StreamType::BANK_DUMP:
			// The documentation found in the Internet on the split patches is wrong. It states the Matrix 1000 sends 50, but in reality it sends 0x50 = 80. That is a strange number.
			// Bob from Tauntek confirmed the assembler code of the M1K uses $50, so that error probably has been there since the 80s.
			return found == numberOfPatches() && split == 0x50 && master > 0;
		default:
			return true;
		}
	}

	bool Matrix1000::shouldStreamAdvance(std::vector<MidiMessage> const &messages, StreamType streamType) const
	{
		ignoreUnused(messages, streamType);
		// The Matrix 1000 does not need encouragement to send the next stream element, so we can safely return false here
		return false;
	}

	midikraft::TPatchVector Matrix1000::loadPatchesFromStream(std::vector<MidiMessage> const &sysexMessages) const
	{
		TPatchVector result;
		for (auto message : sysexMessages) {
			if (isSingleProgramDump({ message })) {
				result.push_back(patchFromProgramDumpSysex({ message }));
			}
			else if (isEditBufferDump({ message })) {
				// This code will be reached for the message format "single patch data to edit buffer", which the M1k will never generate, but I will
				result.push_back(patchFromSysex({ message }));
			}
			else if (isSplitPatch(message) || globalSettingsLoader_->isDataFile(message, 0)) {
				// Ignore other messages like global settings and fake split patches
			}
			else {
				SimpleLogger::instance()->postMessage("Matrix 1000: Ignoring sysex message found, not implemented: " + message.getDescription());
			}
		}
		return result;
	}

	bool Matrix1000::isSplitPatch(MidiMessage const &message) const {
		// The Matrix 1000 does not support split patches, but for the sake of compatibility with the Matrix 6 it will send out 50 split patches as answer to the request dump, 
		// which shall be ignored...
		// Format: F0H 10H 06H 02H <number> <36 bytes of data> <checksum> F7H.
		return isOwnSysex(message) && message.getSysExDataSize() >2 && message.getSysExData()[2] == 0x02;
	}

	juce::MidiMessage Matrix1000::saveEditBufferToProgram(int programNumber)
	{
		jassert(programNumber >= 0 && programNumber < 1000);
		uint8 bank = (uint8)(programNumber / 100);
		uint8 num = (uint8)(programNumber % 100);
		return MidiHelpers::sysexMessage({ MIDI_ID.OBERHEIM, MIDI_ID.MATRIX6_1000, MIDI_COMMAND.STORE_EDIT_BUFFER, num, bank, 0 /* this says group mode off */ });
	}

	std::vector<juce::MidiMessage> Matrix1000::requestPatch(int programNumber) const
	{
		std::vector<MidiMessage> result;
		jassert(programNumber >= 0 && programNumber < 1000);
		uint8 bank = (uint8)(programNumber / 100);
		uint8 num = (uint8)(programNumber % 100);
		result.push_back(createBankSelect(MidiBankNumber::fromZeroBase(bank, 100)));
		result.push_back(createBankUnlock());
		result.push_back(createRequest(SINGLE_PATCH, num));
		return result;
	}

	std::shared_ptr<DataFile> Matrix1000::patchFromSysex(const std::vector<MidiMessage>& message) const
	{
		if (!isEditBufferDump(message)) {
			jassert(false);
			return std::make_shared<Matrix1000Patch>(PatchData(), MidiProgramNumber());
		}

		// Decode the data
		const uint8 *startOfData = &message[0].getSysExData()[4];
		auto matrixPatch = std::make_shared<Matrix1000Patch>(unescapeSysex(startOfData, message[0].getSysExDataSize() - 4), getProgramNumber(message));
		return matrixPatch;
	}

	std::shared_ptr<DataFile> Matrix1000::patchFromPatchData(const Synth::PatchData &data, MidiProgramNumber place) const {
		auto newPatch = std::make_shared<Matrix1000Patch>(data, place);
		return newPatch;
	}


	Synth::PatchData Matrix1000::filterVoiceRelevantData(std::shared_ptr<DataFile> unfilteredData) const
	{
		// The first 8 bytes are the name in the sysex data, and will be cleared out by the Matrix 1000 if you send a patch to it and then back again
		// So we must ignore them by setting them to 0!
		return Patch::blankOut(kMatrix1000BlankOutZones, unfilteredData->data());
	}

	bool Matrix1000::canChangeInputChannel() const
	{
		//TODO - actually you can do that, but it requires a complete roundtrip to query the global page, change the channel, and send the changed page back.
		return false;
	}

	void Matrix1000::changeInputChannel(MidiController *controller, MidiChannel channel, std::function<void()> onFinished)
	{
		ignoreUnused(controller, channel, onFinished);
		throw new std::runtime_error("Illegal state");
	}

	MidiChannel Matrix1000::getInputChannel() const
	{
		return channel();
	}

	bool Matrix1000::hasMidiControl() const
	{
		return false;
	}

	bool Matrix1000::isMidiControlOn() const
	{
		return true;
	}

	void Matrix1000::setMidiControl(MidiController *controller, bool isOn)
	{
		ignoreUnused(controller, isOn);
		throw new std::runtime_error("Illegal state");
	}

	/*std::vector<std::string> Matrix1000::presetNames()
	{
		auto basename = (boost::format("Knobkraft M1000 %d") % channel().toOneBasedInt()).str();
		return { basename + " Main", basename + " Second" };
	}*/

	std::vector<juce::MidiMessage> Matrix1000::deviceDetect(int channel)
	{
		std::vector<uint8> sysEx({ 0x7e, (uint8)channel, 0x06, 0x01 });
		return { MidiMessage::createSysExMessage(&sysEx[0], (int)sysEx.size()) };
	}

	int Matrix1000::deviceDetectSleepMS()
	{
		// The Matrix1000 can be sluggish to react on a Device ID request, better wait for 200 ms
		return 200;
	}

	MidiChannel Matrix1000::channelIfValidDeviceResponse(const MidiMessage &message)
	{
		auto data = message.getSysExData();
		if (message.getSysExDataSize() < 3)
			return MidiChannel::invalidChannel();

		if (message.getSysExDataSize() == 13 &&
			data[0] == 0x7e &&
			data[2] == 0x06 &&
			data[3] == 0x02 &&
			data[4] == MIDI_ID.OBERHEIM &&
			data[5] == MIDI_ID.MATRIX6_1000 &&
			data[6] == 0x00 &&
			//data[7] == 0x02 && // Fam member = Matrix 1000
			data[8] == 0x00) {
			return MidiChannel::fromZeroBase(data[1]); // Return the channel reported by the Matrix
		}
		// Characters 9 to 12 specify the firmware revision
		return MidiChannel::invalidChannel();
	}

	bool Matrix1000::needsChannelSpecificDetection()
	{
		return true;
	}

	void Matrix1000::setGlobalSettingsFromDataFile(std::shared_ptr<DataFile> dataFile)
	{
		auto settingsArray = unescapeSysex(dataFile->data().data(), (int)dataFile->data().size());
		if (settingsArray.size() == 172) {
			updateSynthWithGlobalSettingsListener_.globalSettingsData_ = settingsArray;
			for (size_t i = 0; i < sMatrix1000GlobalSettings()->definitions.size(); i++) {
				if (i < settingsArray.size()) {
					int intValue = settingsArray[sMatrix1000GlobalSettings()->definitions[i].sysexIndex] + sMatrix1000GlobalSettings()->definitions[i].displayOffset;
					if (sMatrix1000GlobalSettings()->definitions[i].isTwosComplement) {
						// Very special code that only works because there are just two fields in the global settings that need it
						// Master transpose and Master tuning
						if (intValue > 127) {
							intValue = (int8)intValue;
						}
					}
					globalSettings_[i]->value().setValue(var(intValue));
				}
			}
		}
		else {
			SimpleLogger::instance()->postMessage("Ignoring Matrix1000 global settings data - unescaped block size is not 172 bytes");
			jassert(settingsArray.size() == 172);
		}
	}

	std::vector<std::shared_ptr<TypedNamedValue>> Matrix1000::getGlobalSettings()
	{
		return globalSettings_;
	}

	midikraft::DataFileLoadCapability * Matrix1000::loader()
	{
		return globalSettingsLoader_;
	}

	int Matrix1000::settingsDataFileType() const
	{
		return DF_MATRIX1000_SETTINGS;
	}

	std::vector<juce::MidiMessage> Matrix1000::patchToSysex(std::shared_ptr<DataFile> patch) const
	{
		std::vector<uint8> editBufferDump({ MIDI_ID.OBERHEIM, MIDI_ID.MATRIX6_1000, MIDI_COMMAND.SINGLE_PATCH_TO_EDIT_BUFFER, 0x00 /* Unspecified, but let's assume 0 is ok */ });
		auto patchdata = escapeSysex(patch->data());
		std::copy(patchdata.begin(), patchdata.end(), std::back_inserter(editBufferDump));
		return std::vector<MidiMessage>({ MidiHelpers::sysexMessage(editBufferDump) });
	}

	Matrix1000::Matrix1000() : updateSynthWithGlobalSettingsListener_(this)
	{
		globalSettingsLoader_ = new Matrix1000_GlobalSettings_Loader(this);
		initGlobalSettings();
	}

	Matrix1000::~Matrix1000()
	{
		delete globalSettingsLoader_;
	}

	std::string Matrix1000::getName() const
	{
		return "Oberheim Matrix 1000";
	}

	Synth::PatchData Matrix1000::unescapeSysex(const uint8 *sysExData, int sysExLen) const
	{
		Synth::PatchData result;

		// The Matrix 1000 does two things: Calculate a checksum (yes, it's a sum) and pack each byte into two nibbles. That's not really
		// data efficient, but hey, a 2 MHz 8-bit CPU must be able to pack and unpack that at MIDI speed!
		uint8 checksum = 0;
		for (int index = 0; index < sysExLen; ) {
			// Read one more data byte from two bytes
			uint8 byte = sysExData[index] | sysExData[index + 1] << 4;
			result.push_back(byte);
			checksum += byte;
			index += 2;
			if (index == sysExLen - 1) {
				// This is the checksum!
				if (sysExData[index] != (checksum & 0x7f)) {
					// Invalid checksum, don't use this
					result.clear();
					return result;
				}
				index++;
			}
		}
		return result;
	}

	std::vector<juce::uint8> Matrix1000::escapeSysex(const PatchData &programEditBuffer) const
	{
		std::vector<uint8> result;
		// We generate the nibbles and the checksum
		int checksum = 0;
		for (int i = 0; i < programEditBuffer.size(); i++) {
			checksum += programEditBuffer[i];
			result.push_back(programEditBuffer[i] & 0x0f);
			result.push_back((programEditBuffer[i] & 0xf0) >> 4);
		}
		result.push_back(checksum & 0x7f);
		return result;
	}

	/*void Matrix1000::setupBCR2000(MidiController *controller, BCR2000 &bcr, SimpleLogger *logger) {
		if (bcr.wasDetected() && wasDetected()) {
			// Make sure to bake the current channel of the Matrix into the setup for the BCR
			Matrix1000BCR sysex_bcr(channel().toZeroBasedInt());
			Matrix1000BCR nrpn_bcr(channel().toZeroBasedInt(), false);

			// Now, manually build both again with only one header and footer
			std::string bothPresets;
			bothPresets = BCR2000::generateBCRHeader();
			bothPresets += sysex_bcr.generateBCR(presetNames()[0], presetNames()[1], BCR2000_Preset_Positions::MATRIX1000_P1, false);
			//bothPresets += nrpn_bcr.generateBCR(3, false);
			bothPresets += BCR2000::generateBCREnd(BCR2000_Preset_Positions::MATRIX1000_P1);
			controller->enableMidiInput(bcr.midiInput());
			bcr.sendSysExToBCR(controller->getMidiOutput(bcr.midiOutput()), bcr.convertToSyx(bothPresets), *controller, logger);
		}
	}

	void Matrix1000::syncDumpToBCR(MidiProgramNumber programNumber, MidiController *controller, BCR2000 &bcr, SimpleLogger *logger) {
		if (matrixBCRSyncHandler_.is_nil()) {
			matrixBCRSyncHandler_ = EditBufferHandler::makeOne();
			controller->setNextEditBufferHandler(matrixBCRSyncHandler_, [this, logger, controller, &bcr](MidiMessage const &message) {
				if (isEditBufferDump(message)) {
					logger->postMessage("Got Edit Buffer from Matrix and now updating the BCR2000");
					auto patch = std::dynamic_pointer_cast<Matrix1000Patch>(patchFromSysex(message));

					// Loop over all parameters in patch and send out the appropriate NRPNs, if they are within the BCR2000 definition...
					std::vector<MidiMessage> updates;
					for (auto paramDef : patch->allParameterDefinitions()) {
						auto m1000p = dynamic_cast<Matrix1000ParamDefinition *>(paramDef);
						if (m1000p != nullptr) {
							if (Matrix1000BCR::parameterHasControlAssigned(m1000p->id()) && !(m1000p->bitposition() != -1)) {
								// Remember to not create NRPN messages for sub-bit-parameters - else we would create multiple messages for the same sysex byte
								auto nrpn = Matrix1000BCR::createNRPNforParam(dynamic_cast<Matrix1000ParamDefinition *>(paramDef), *patch, channel().toZeroBasedInt());
								std::copy(nrpn.begin(), nrpn.end(), std::back_inserter(updates));
							}
						}
					}
					MidiBuffer updateBuffer = MidiHelpers::bufferFromMessages(updates);
					// Send this to BCR
					controller->getMidiOutput(bcr.midiOutput())->sendBlockOfMessagesNow(updateBuffer);
				}
			});
		}
		// We want to talk to the Matrix now
		controller->enableMidiInput(midiInput());
		controller->getMidiOutput(midiOutput())->sendMessageNow(requestEditBufferDump());
	}

	void Matrix1000::setupBCR2000View(BCR2000_Component &view) {
		//TODO
	}

	void Matrix1000::setupBCR2000Values(BCR2000_Component &view, Patch *patch)
	{
		//TODO
	}
	*/
}