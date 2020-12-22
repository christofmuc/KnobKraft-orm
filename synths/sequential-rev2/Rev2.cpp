/*
   Copyright (c) 2019 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "Rev2.h"

#include "Patch.h"

#include "Rev2Patch.h"

#include <algorithm>
#include <boost/format.hpp>

#include "MidiHelpers.h"
#include "TypedNamedValue.h"
#include "MidiTuning.h"
#include "MTSFile.h"

namespace midikraft {

	// Definitions for the SysEx format we need
	const size_t cGatedSeqOnIndex = 139;
	const size_t cGatedSeqDestination = 111;
	const size_t cGatedSeqIndex = 140;
	const size_t cStepSeqNote1Index = 256;
	const size_t cStepSeqVelocity1Index = 320;
	const size_t cLayerB = 2048 / 2;
	const size_t cABMode = 231;
	const size_t cBpmTempo = 130;
	const size_t cClockDivide = 131;

	// Some constants
	const uint8 cDefaultNote = 0x3c;

	std::vector<Range<int>> kRev2BlankOutZones = {
		{ 211, 231 }, // unused according to doc
		{ 1235, 1255 }, // same in layer B
		{ 235, 255 }, // name of layer A
		{ 1259, 1279 }, // name of layer B
		{ 2044, 2047} // the two bytes that are wrongly not encoded (firmware bug), and two bytes that are only buffered to get to clean 2048 size
	};

	std::string intervalToText(int interval) {
		if (interval == 0) {
			return "same note";
		}
		int octave = 0;
		while (interval >= 12) {
			octave++;
			interval -= 12;
		}
		std::string octaveText = octave == 1 ? "one octave" : (boost::format("%d octaves") % octave).str();
		std::string semitoneText = interval == 0 ? "same note" : (boost::format("%d semi-tones") % interval).str();
		if (interval == 0) {
			return octaveText;
		}
		else if (octave == 0) {
			return semitoneText;
		}
		else {
			return octaveText + " and " + semitoneText;
		}
	}

	Rev2::Rev2() : DSISynth(0x2f /* Rev2 ID */)
	{
		initGlobalSettings();
	}

	Synth::PatchData Rev2::filterVoiceRelevantData(std::shared_ptr<DataFile> unfilteredData) const
	{
		switch (unfilteredData->dataTypeID())
		{
		case PATCH:
			return Patch::blankOut(kRev2BlankOutZones, unfilteredData->data());
		case GLOBAL_SETTINGS:
			// Global settings don't contain a name, all data is relevant
			return unfilteredData->data();
		case ALTERNATE_TUNING:
			//TBD - name irrelevant?
			return unfilteredData->data();
		default:
			throw new std::runtime_error("Invalid argument - unknown data type id");
		}
	}

	int Rev2::numberOfBanks() const
	{
		return 8;
	}

	int Rev2::numberOfPatches() const
	{
		return 128;
	}

	std::string Rev2::friendlyBankName(MidiBankNumber bankNo) const
	{
		int section = bankNo.toZeroBased() / 4;
		int bank = bankNo.toZeroBased();
		return (boost::format("%s%d") % (section == 0 ? "U" : "F") % ((bank % 4) + 1)).str();
	}

	std::shared_ptr<Patch> Rev2::patchFromSysex(const MidiMessage& message) const
	{
		int startIndex = -1;

		if (isEditBufferDump(message)) {
			startIndex = 3;
		}

		if (isSingleProgramDump(message)) {
			startIndex = 5;
		}

		if (startIndex == -1) {
			// ups
			jassert(false);
			return nullptr;
		}

		// Decode the data
		const uint8 *startOfData = &message.getSysExData()[startIndex];
		auto patchData = unescapeSysex(startOfData, message.getSysExDataSize() - startIndex, 2048);
		MidiProgramNumber place;
		if (isSingleProgramDump(message)) {
			int bank = message.getSysExData()[3];
			int program = message.getSysExData()[4];
			place = MidiProgramNumber::fromZeroBase(bank * 128 + program);
		}
		auto patch = std::make_shared<Rev2Patch>(patchData, place);

		return patch;
	}

	std::shared_ptr<DataFile> Rev2::patchFromPatchData(const Synth::PatchData &data, MidiProgramNumber place) const {
		ignoreUnused(place);

		//TODO - this is a hack. We should only store MIDI messages in the database, should we?
		// Recreate a MIDI message from the bytes given, and test if it is a valid datafile...
		auto message = MidiMessage::createSysExMessage(data.data(), (int) data.size());
		if (isDataFile(message, GLOBAL_SETTINGS)) {
			auto result = loadData({ message }, GLOBAL_SETTINGS);
			return result[0];
		}
		else if (isDataFile(message, ALTERNATE_TUNING)) {
			auto result = loadData({ message }, ALTERNATE_TUNING);
			return result[0];
		}

		return std::make_shared<Rev2Patch>(data, place);
	}


	std::vector<juce::MidiMessage> Rev2::patchToSysex(const Patch &patch) const
	{
		// By default, create an edit buffer dump file...
		std::vector<uint8> programEditBufferDataDump({ 0x01 /* DSI */, midiModelID_, 0x03 /* Edit Buffer Data */ });
		jassert(patch.data().size() == 2046 || patch.data().size() == 2048); // Original size is 2046, but to find some programming errors at some points I buffer to 2048
		auto patchData = escapeSysex(patch.data(), 2046);
		jassert(patchData.size() == 2339);
		std::copy(patchData.begin(), patchData.end(), std::back_inserter(programEditBufferDataDump));
		return std::vector<MidiMessage>({ MidiHelpers::sysexMessage(programEditBufferDataDump) });
	}

	uint8 Rev2::clamp(int value, uint8 minimum /* = 9*/, uint8 maximum /* = 127 */) {
		return static_cast<uint8>(std::min((int)maximum, std::max(value, (int)minimum)));
	}

	juce::MidiMessage Rev2::filterProgramEditBuffer(const MidiMessage &programEditBuffer, std::function<void(std::vector<uint8> &)> filterExpressionInPlace) {
		if (!isEditBufferDump(programEditBuffer)) {
			jassert(false);
			return MidiMessage(); // Empty sysex message so it doesn't crash
		}

		// Decode the data
		const uint8 *startOfData = &programEditBuffer.getSysExData()[3];
		std::vector<uint8> programEditBufferDecoded = unescapeSysex(startOfData, programEditBuffer.getSysExDataSize() - 3, 2048);

		// Perform filter operation
		filterExpressionInPlace(programEditBufferDecoded);

		return buildSysexFromEditBuffer(programEditBufferDecoded);
	}

	juce::MidiMessage Rev2::buildSysexFromEditBuffer(std::vector<uint8> editBuffer) {
		// Done, now create a new encoded buffer
		std::vector<uint8> encodedBuffer = escapeSysex(editBuffer, 2046);

		// Build the sysex method with the patched buffer
		std::vector<uint8> sysEx({ 0b00000001, 0b00101111, 0b00000011 });
		sysEx.insert(sysEx.end(), encodedBuffer.begin(), encodedBuffer.end());
		auto result = MidiMessage::createSysExMessage(&sysEx.at(0), (int)sysEx.size());
		return result;
	}

	bool isPolySequencerRest(int note, int velocity) {
		// Wild guess...
		return note == 60 && velocity == 128;
	}

	bool isPolySequencerTie(int note, int velocity) {
		ignoreUnused(velocity);
		return note > 128;
	}

	juce::MidiMessage Rev2::patchPolySequenceToGatedTrack(const MidiMessage& message, int gatedSeqTrack)
	{
		return filterProgramEditBuffer(message, [gatedSeqTrack](std::vector<uint8> &programEditBuffer) {
			// Copy the PolySequence into the Gated Track
			// Find the lowest note in the poly sequence
			int lowestNote = 127;
			for (int i = 0; i < 16; i++) {
				if (programEditBuffer[cStepSeqNote1Index + i] < lowestNote) {
					lowestNote = programEditBuffer[cStepSeqNote1Index + i];
				}
			}

			// As the Gated Sequencer only has positive values, and I want the key to be the first key of the sequence, our only choice is
			// to move up a few octaves so we stay in key...
			int indexNote = programEditBuffer[cStepSeqNote1Index];
			while (lowestNote < indexNote) {
				indexNote -= 12;
			}

			for (int i = 0; i < 16; i++) {
				// 16 steps in the gated sequencer...
				// The gated sequencer allows half-half steps in pitch, so we multiply by 2...
				uint8 notePlayed = programEditBuffer[cStepSeqNote1Index + i];
				uint8 velocityPlayed = programEditBuffer[cStepSeqVelocity1Index + i];
				if (velocityPlayed > 0 && !isPolySequencerRest(notePlayed, velocityPlayed) && !isPolySequencerTie(notePlayed, velocityPlayed)) {
					programEditBuffer[gatedSeqTrack * 16 + i + cGatedSeqIndex] = clamp((notePlayed - indexNote) * 2, 0, 125);
				}
				else {
					// 126 is the reset in the gated sequencer, 127 is the rest, which is only allowed in track 1 if I believe the Prophet 8 documentation
					programEditBuffer[gatedSeqTrack * 16 + i + cGatedSeqIndex] = 127;
				}
				programEditBuffer[(gatedSeqTrack + 1) * 16 + i + cGatedSeqIndex] = clamp(velocityPlayed / 2, 0, 125);
			}

			// Poke the sequencer on and set the destination to OscAllFreq
			programEditBuffer[cGatedSeqOnIndex] = 0; // 0 is gated sequencer, 1 is poly sequencer
			programEditBuffer[cGatedSeqDestination] = 3;

			// If we are in a stacked program, we copy layer A to B so both sounds get the same sequence
			if (programEditBuffer[cABMode] == 1) {
				programEditBuffer[cLayerB + cGatedSeqDestination] = programEditBuffer[cGatedSeqDestination];
				programEditBuffer[cLayerB + cGatedSeqOnIndex] = programEditBuffer[cGatedSeqOnIndex];
				std::copy(std::next(programEditBuffer.begin(), cGatedSeqIndex),
					std::next(programEditBuffer.begin(), cGatedSeqIndex + 4 * 16),
					std::next(programEditBuffer.begin(), cLayerB + cGatedSeqIndex));

				// And we should make sure that the bpm and clock divide is the same on layer B
				programEditBuffer[cLayerB + cBpmTempo] = programEditBuffer[cBpmTempo];
				programEditBuffer[cLayerB + cClockDivide] = programEditBuffer[cClockDivide];
			}
		});
	}

	juce::MidiMessage Rev2::copySequencersFromOther(const MidiMessage& currentProgram, const MidiMessage &lockedProgram)
	{
		// Decode locked data as well
		jassert(isEditBufferDump(lockedProgram));
		const uint8 *startOfData = &lockedProgram.getSysExData()[3];
		std::vector<uint8> lockedProgramBufferDecoded = unescapeSysex(startOfData, lockedProgram.getSysExDataSize() - 3, 2048);
		return filterProgramEditBuffer(currentProgram, [lockedProgramBufferDecoded](std::vector<uint8> &programEditBuffer) {
			// Copy poly sequence of both layers, 6 tracks with 64 bytes for note and 64 bytes for velocity each!
			std::copy(std::next(lockedProgramBufferDecoded.begin(), cStepSeqNote1Index),
				std::next(lockedProgramBufferDecoded.begin(), cStepSeqNote1Index + 6 * 64 * 2),
				std::next(programEditBuffer.begin(), cStepSeqNote1Index));
			std::copy(std::next(lockedProgramBufferDecoded.begin(), cLayerB + cStepSeqNote1Index),
				std::next(lockedProgramBufferDecoded.begin(), cLayerB + cStepSeqNote1Index + 6 * 64 * 2),
				std::next(programEditBuffer.begin(), cLayerB + cStepSeqNote1Index));

			// Copy 4 tracks with 16 bytes each for the gated sequencer
			std::copy(std::next(lockedProgramBufferDecoded.begin(), cGatedSeqIndex),
				std::next(lockedProgramBufferDecoded.begin(), cGatedSeqIndex + 4 * 16),
				std::next(programEditBuffer.begin(), cGatedSeqIndex));
			std::copy(std::next(lockedProgramBufferDecoded.begin(), cLayerB + cGatedSeqIndex),
				std::next(lockedProgramBufferDecoded.begin(), cLayerB + cGatedSeqIndex + 4 * 16),
				std::next(programEditBuffer.begin(), cLayerB + cGatedSeqIndex));

			// For the gated to work as expected, take over the switch as well which of the sequencers is on (poly or gated),
			// and we need the gated destination for track 1 to be osc all frequencies
			programEditBuffer[cGatedSeqOnIndex] = lockedProgramBufferDecoded[cGatedSeqOnIndex];
			programEditBuffer[cGatedSeqDestination] = lockedProgramBufferDecoded[cGatedSeqDestination];
			programEditBuffer[cLayerB + cGatedSeqOnIndex] = lockedProgramBufferDecoded[cLayerB + cGatedSeqOnIndex];
			programEditBuffer[cLayerB + cGatedSeqDestination] = lockedProgramBufferDecoded[cLayerB + cGatedSeqDestination];

			// Also copy over tempo and clock
			programEditBuffer[cBpmTempo] = lockedProgramBufferDecoded[cBpmTempo];
			programEditBuffer[cClockDivide] = lockedProgramBufferDecoded[cClockDivide];
			programEditBuffer[cLayerB + cBpmTempo] = lockedProgramBufferDecoded[cLayerB + cBpmTempo];
			programEditBuffer[cLayerB + cClockDivide] = lockedProgramBufferDecoded[cLayerB + cClockDivide];
		});
	}

	void Rev2::switchToLayer(int layerNo)
	{
		if (wasDetected()) {
			// The Rev2 has only two layers, A and B
			// Which of the layers is played is not part of the patch data, but is a global setting/parameter. Luckily, this can be switched via an NRPN message
			// The DSI synths like MSB before LSB
			auto messages = MidiHelpers::generateRPN(channel().toOneBasedInt(), 4190, layerNo, true, true, true);
			sendBlockOfMessagesToSynth(midiOutput(), MidiHelpers::bufferFromMessages(messages));
		}
	}

	MidiBuffer Rev2::layerToSysex(std::shared_ptr<DataFile> const patch, int sourceLayer, int targetLayer) const
	{
		ignoreUnused(targetLayer);
		MidiBuffer allMessages;
		// Now, these will be a lot of NRPN messages generated, but what we can do is to generate a layer change by settings all values of all parameters via NRPN
		auto rev2patch = std::dynamic_pointer_cast<Rev2Patch>(patch);
		if (rev2patch) {
			// Loop all parameters, and create set value messages for them
			int deltaTimeInSamples = 0;
			int count = 0;
			for (auto param : rev2patch->allParameterDefinitions()) {
				count++;
				if (count > 88) break;
				auto rev2param = std::dynamic_pointer_cast<Rev2ParamDefinition>(param);
				if (rev2param) {
					rev2param->setSourceLayer(sourceLayer);
					rev2param->setTargetLayer(targetLayer);
					auto p = std::dynamic_pointer_cast<Patch>(patch);
					if (p) {
						allMessages.addEvents(rev2param->setValueMessages(p, this), 0, -1, deltaTimeInSamples);
						deltaTimeInSamples += 10;
					}
				}
			}
		}
		return allMessages;
	}

	void Rev2::changeInputChannel(MidiController *controller, MidiChannel newChannel, std::function<void()> onFinished)
	{
		ignoreUnused(controller);
		// The Rev2 will change its channel with a nice NRPN message
		// See page 87 of the manual
		// Setting it to 0 would be Omni, so we use one based int
		sendBlockOfMessagesToSynth(midiOutput(), createNRPN(4098, newChannel.toOneBasedInt()));
		setCurrentChannelZeroBased(midiInput(), midiOutput(), newChannel.toZeroBasedInt());
		onFinished();
	}

	void Rev2::setMidiControl(MidiController *controller, bool isOn)
	{
		ignoreUnused(controller);
		// See page 87 of the manual
		sendBlockOfMessagesToSynth(midiOutput(), createNRPN(4103, isOn ? 1 : 0));
		localControl_ = isOn;
	}

	void Rev2::changeOutputChannel(MidiController *controller, MidiChannel channel, std::function<void()> onFinished)
	{
		// The Rev2 has no split input and output channel. So MIDI routing is vital in this case.
		changeInputChannel(controller, channel, onFinished);
	}

	MidiNote Rev2::getLowestKey() const
	{
		return MidiNote(0x24);
	}

	MidiNote Rev2::getHighestKey() const
	{
		return MidiNote(0x60);
	}

	std::vector<juce::MidiMessage> Rev2::requestDataItem(int itemNo, int dataTypeID)
	{
		switch (dataTypeID) {
		case GLOBAL_SETTINGS:
			return { MidiHelpers::sysexMessage({ 0b00000001, midiModelID_, 0b00001110 /* Request global parameter transmit */ }) };
		case ALTERNATE_TUNING:
			return { MidiTuning::createTuningDumpRequest(0x01, MidiProgramNumber::fromZeroBase(itemNo)) };
		default:
			// Undefined
			jassert(false);
		}
		return {};
	}

	int Rev2::numberOfDataItemsPerType(int dataTypeID) const
	{
		switch (dataTypeID) {
		case GLOBAL_SETTINGS:
			return 1;
		case ALTERNATE_TUNING:
			return 17;
		default:
			jassert(false);
		}
		return 0;
	}

	bool Rev2::isDataFile(const MidiMessage &message, int dataTypeID) const
	{
		switch (dataTypeID)
		{
		case PATCH:
			//TODO Patch Loading is done via the Edit Buffer mechanism
			return false;
		case GLOBAL_SETTINGS:
			if (isOwnSysex(message)) {
				if (message.getSysExDataSize() > 2 && message.getSysExData()[2] == 0b00001111 /* Main Parameter Data*/) {
					return true;
				}
			}
			break;
		case ALTERNATE_TUNING:
			return MidiTuning::isTuningDump(message);
		default:
			jassert(false);
		}
		return false;
	}

	struct Rev2GlobalSettings {
		std::vector<DSIGlobalSettingDefinition> definitions = {
			{ 0, 4097, { "Master Coarse Tune", "Tuning", 12, -12, 12 }, -12 }, // Default 12, displayed as 0
			{ 1, 4096, { "Master Fine Tune", "Tuning", 25, -50, 50 }, -50 }, // Default 50, displayed as 0
			{ 2, 4098, { "MIDI Channel", "MIDI", 0, { {0, "Omni"}, {1, "1" }, {2, "2" }, {3, "3" }, {4, "4" }, {5, "5" }, {6, "6" }, {7, "7" }, {8, "8" }, {9, "9" }, {10, "10" }, {11, "11" }, {12, "12" }, {13, "13" }, {14, "14" }, {15, "15" }, {16, "16" }} } },
			{ 3, 4099, { "MIDI Clock Mode", "MIDI", 1, { {0, "Off"}, { 1, "Master" }, { 2, "Slave" }, { 3, "Slave Thru" }, { 4, "Slave No S/S"} } } },
			{ 4, 4100, { "MIDI Clock Cable", "MIDI", 0, { {0, "MIDI"}, { 1, "USB" } } } },
			{ 5, 4101, { "MIDI Param Send", "MIDI", 2, { {0, "Off"}, { 1, "CC" }, { 2, "NRPN"} } } },
			{ 6, 4102, { "MIDI Param Receive", "MIDI", 2, { {0, "Off"}, { 1, "CC" }, { 2, "NRPN"} } } },
			{ 7, 4103, { "MIDI Control Enable", "MIDI", true } },
			//{ 8, { "UNKNOWN", "MIDI", Value(), ValueType::Integer, 0, 100 } },
			{ 22, 4118, { "MIDI Prog Enable", "MIDI", true } },
			{ 26, 4125, { "MIDI Prog Send", "MIDI", true } },
			{ 10, 4104, { "MIDI Sysex Cable", "MIDI", 0, { {0, "MIDI"}, { 1, "USB" } } } },
			{ 9, 4105, { "MIDI Out Select", "MIDI", 2, { { 0, "MIDI" }, { 1, "USB"}, { 2, "MIDI+USB" } } } },
			{ 11, 4123, { "MIDI Arp+Seq", "MIDI", true } },
			{ 25, 4124, { "Arp Beat Sync", "MIDI", 0, { {0, "Off"}, { 1, "Quantize" } } } },
			{ 21, 4119, { "MIDI MultiMode Enabled", "MIDI", true } },
			{ 12, 4107, { "Local Control Enabled", "MIDI", true } },
			{ 17, 4113, { "Velocity Curve", "Keyboard", 0, { {0, "Curve 1" }, {1, "Curve 2" }, {2, "Curve 3" }, {3, "Curve 4" }, {4, "Curve 5" }, {5, "Curve 6" }, {6, "Curve 7" }, {7, "Curve 8" } } } },
			{ 18, 4114, { "Pressure Curve", "Keyboard", 0, { {0, "Curve 1" }, {1, "Curve 2" }, {2, "Curve 3" }, {3, "Curve 4" }  } } },
			{ 19, 4115, { "Stereo or Mono", "Audio Setup", 0, { {0, "Stereo" }, { 1, "Mono" } } } },
			{ 14, 4109, { "Pot Mode", "Front controls", 0, { {0, "Relative"}, { 1, "Pass Thru" }, { 2, "Jump" } } } },
			{ 16, 4116, { "Alternative Tuning", "Scales", 0, kDSIAlternateTunings() } },
			{ 20, 4120, { "Screen Saver Enabled", "General", true } },
			{ 13, 4111, { "Seq Pedal Mode", "Controls", 0, { {0, "Normal"}, { 1, "Trigger" }, { 2, "Gate" }, { 3, "Trigger+Gate" } } } },
			{ 24, 4122, { "Foot Assign", "Controls", 0, { { 0, "Breath CC2" }, { 1, "Foot CC4" }, { 2, "Exp CC11" }, { 3, "Volume" }, { 4, "LPF Full" }, { 5, "LPF Half" } } } },
			{ 15, 4112, { "Sustain polarity", "Controls", 0, { {0, "Normal"}, { 1, "Reversed" } } } },
			{ 23, 4121, { "Sustain Arp", "Controls", 0, { {0, "Arp Hold"}, { 1, "Sustain" }, { 2, "Arp Hold Mom" } } } },
			{ 27, 4126, { "Save Edit B Enabled", "Controls", true } },
		};
	};
	std::shared_ptr<Rev2GlobalSettings> sRev2GlobalSettings;
	std::vector<DSIGlobalSettingDefinition> &gRev2GlobalSettings() {
		if (!sRev2GlobalSettings) {
			sRev2GlobalSettings = std::make_shared<Rev2GlobalSettings>();
		}
		return sRev2GlobalSettings->definitions;
	}

	void Rev2::initGlobalSettings()
	{
		//TODO could this go into the DSISynth class as well?
		// Loop over it and fill out the GlobalSettings Properties
		globalSettings_.clear();
		for (size_t i = 0; i < gRev2GlobalSettings().size(); i++) {
			auto setting = std::make_shared<TypedNamedValue>(gRev2GlobalSettings()[i].typedNamedValue);
			globalSettings_.push_back(setting);
		}
		globalSettingsTree_ = ValueTree("REV2SETTINGS");
		globalSettings_.addToValueTree(globalSettingsTree_);
		globalSettingsTree_.addListener(&updateSynthWithGlobalSettingsListener_);
	}

	std::vector<std::shared_ptr<DataFile>> Rev2::loadData(std::vector<MidiMessage> messages, int dataTypeID) const
	{
		std::vector<std::shared_ptr<DataFile>> result;
		for (auto m : messages) {
			if (isDataFile(m, dataTypeID)) {
				switch (dataTypeID) {
				case GLOBAL_SETTINGS: {
					std::vector<uint8> syx(m.getSysExData(), m.getSysExData() + m.getSysExDataSize());
					auto storage = std::make_shared<MTSFile>(GLOBAL_SETTINGS, syx);
					result.push_back(storage);
					break;
				}
				case ALTERNATE_TUNING: {
					MidiTuning tuning(MidiProgramNumber::fromZeroBase(0), "unused", {});
					if (MidiTuning::fromMidiMessage(m, tuning)) {
						std::vector<uint8> mtsData({ m.getSysExData(), m.getSysExData() + m.getSysExDataSize() });
						auto storage = std::make_shared<MTSFile>(ALTERNATE_TUNING, mtsData);
						result.push_back(storage);
					}
					else {
						jassert(false);
					}
					break;
				}
				default:
					jassert(false);
				}
			}
		}
		return result;
	}

	std::vector<DataFileLoadCapability::DataFileDescription> Rev2::dataTypeNames() const
	{
		return { { "Patch", true, true}, { "Global Settings", true, false}, { "Alternate Tuning", false, true } };
	}

	std::vector<juce::MidiMessage> Rev2::dataFileToMessages(std::shared_ptr<DataFile> dataFile, std::shared_ptr<SendTarget> target) const
	{
		ignoreUnused(target);
		switch (dataFile->dataTypeID()) {
		case PATCH:
			// You should not come here?
			jassert(false);
			break;
		case GLOBAL_SETTINGS:
			// Not possible
			jassert(false);
			SimpleLogger::instance()->postMessage("Program error - don't try to send global settings in one messages to synth");
			break;
		case ALTERNATE_TUNING: {
			// This makes sense, though we should patch the program place in the program
			auto mts = std::dynamic_pointer_cast<MTSFile>(dataFile);
			jassert(mts);
			if (mts) {
				return mts->createMidiMessagesFromDataFile(MidiProgramNumber::fromOneBase(16));
			}
			break;
		}
		default:
			jassert(false);
		}
		return {};
	}

	midikraft::DataFileLoadCapability * Rev2::loader()
	{
		return this;
	}

	void Rev2::setLocalControl(MidiController *controller, bool localControlOn)
	{
		ignoreUnused(controller);
		sendBlockOfMessagesToSynth(midiOutput(), createNRPN(4107, localControlOn ? 1 : 0));
		localControl_ = localControlOn;
	}

	std::shared_ptr<Patch> Rev2::patchFromProgramDumpSysex(const MidiMessage& message) const
	{
		return patchFromSysex(message);
	}

	std::vector<juce::MidiMessage> Rev2::patchToProgramDumpSysex(const Patch &patch) const
	{
		// Create a program data dump message
		int programPlace = patch.patchNumber()->midiProgramNumber().toZeroBased();
		std::vector<uint8> programDataDump({ 0x01 /* DSI */, midiModelID_, 0x02 /* Program Data */, (uint8) (programPlace / 128), (uint8) (programPlace % 128) });
		auto patchData = escapeSysex(patch.data(), 2046);
		jassert(patchData.size() == 2339);
		std::copy(patchData.begin(), patchData.end(), std::back_inserter(programDataDump));
		return std::vector<MidiMessage>({ MidiHelpers::sysexMessage(programDataDump) });
	}

	std::string Rev2::getName() const
	{
		return "DSI Prophet Rev2";
	}

	juce::MidiMessage Rev2::clearPolySequencer(const MidiMessage &programEditBuffer, bool layerA, bool layerB)
	{
		return filterProgramEditBuffer(programEditBuffer, [layerA, layerB](std::vector<uint8> &programEditBuffer) {
			// Just fill all 6 tracks of the Poly Sequencer with note 0x3f and velocity 0
			for (int track = 0; track < 6; track++) {
				for (int step = 0; step < 64; step++) {
					if (layerA) {
						programEditBuffer[cStepSeqNote1Index + track * 128 + step] = cDefaultNote;
						programEditBuffer[cStepSeqVelocity1Index + track * 128 + step] = 0x00;
					}
					if (layerB) {
						programEditBuffer[cLayerB + cStepSeqNote1Index + track * 128 + step] = cDefaultNote;
						programEditBuffer[cLayerB + cStepSeqVelocity1Index + track * 128 + step] = 0x00;
					}
				}
			}
		});
	}

	int Rev2::settingsDataFileType() const
	{
		return GLOBAL_SETTINGS;
	}

	std::vector<midikraft::DSIGlobalSettingDefinition> Rev2::dsiGlobalSettings() const
	{
		return gRev2GlobalSettings(); //TODO expensive copy constructor of everything
	}

	std::string Rev2::friendlyProgramName(MidiProgramNumber programNo) const
	{
		// The Rev2 has 8 banks of 128 patches, in two sections U and F called U1 to U4 and F1 to F4
		int bank = programNo.toZeroBased() / 128;
		int section = bank / 4;
		int program = programNo.toZeroBased() % 128;
		return (boost::format("%s%d P%d") % (section == 0 ? "U" : "F") % ((bank % 4) + 1) % program).str();
	}

}
