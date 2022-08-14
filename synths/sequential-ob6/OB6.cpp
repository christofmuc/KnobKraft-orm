/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "OB6.h"

#include "OB6Patch.h"

#include "MidiHelpers.h"
#include "MidiController.h"
#include "MidiTuning.h"
#include "MTSFile.h"

#include <boost/format.hpp>

namespace midikraft {

	// The values are  indexes into the global parameter dump
	enum OB6_GLOBAL_PARAMS {
		TRANSPOSE = 0,
		MASTER_TUNE = 1,
		MIDI_CHANNEL = 2,
		MIDI_CLOCK = 3,
		CLOCK_PORT = 4,
		PARAM_TRANSMIT = 5,
		PARAM_RECEIVE = 6,
		MIDI_CONTROL = 7,
		MIDI_SYSEX = 8,
		MIDI_OUT = 9,
		LOCAL_CONTROL = 10,
		SEQ_JACK = 11,
		POT_MODE = 12,
		SUSTAIN_POLARITY = 13,
		ALT_TUNING =14 ,
		VELOCITY_RESPONSE = 15,
		AFTERTOUCH_RESPONSE = 16,
		STEREO_MONO = 17,
		ARP_BEAT_SYNC = 19, // Sadly this is not stored in the byte 18 of the sysex data package. Found it in 19 in firmware 1.6.6. What is in 18?
		MASTER_PITCHBEND_RANGE = 20,
		VOICE_PITCHBEND_RANGE = 21, 
		Y_AXIS_DESTINATION = 22, 
		Y_AXIS_MODE = 23,
		VINTAGE_MODE = 24
	};

	// Warnings for the user
	// 
	// The panel will only work when the parameter "MIDI Param Rcv" is set to NRPN. And if you switch it away, it will stop working.
	// Same with MIDI Control Off - the synth will no longer respond to the NRPN messages we send.
	// Also, the MIDI sysex switch must be set to USB if we talk to the synth via USB

	// Bugs in the OB6 Sysex implementation (V 1.5.8):
	// I documented those here https://forum.sequential.com/index.php/topic,4497.0.html
	//
	// Clock Mode has 5 values, but value "4" cannot be set via NRPN (nSS), only via front panel. It is reported correctly back via global settings dump, though.
	// MIDI Param Xmit has 5 values, but the last value "4" cannot be set via NRPN ("nAS" - NRPN and sequencer)
	// MIDI Out has 4 values, but the last value "3" cannot be set via NRPN
	// Local Control has 2 values, but the last value "1" cannot be set via NRPN (which is bad, because you cannot switch on Local Control remotely with an NRPN)
	// Velocity Response cannot set the highest value either via NRPN
	// Aftertouch Response cannot be set to the highest value either via NRPN
	// Stereo/Mono cannot be set to the highest value "Mono" via NRPN
	// Pot Mode cannot be set to "Jump"
	// Seq Jack cannot be set to "Gate/Trigger"
	// Alt Tuning cannot be set to the highest number
	// Sustain polarity cannot be set to "r-n"
	// 
	// Arp Beat Sync is not written in byte 19 (probably it should be byte 20, and they forgot)
	//
	//
	// Documentation bugs
	// ARP_BEAT_SYNC 1036 is not documented. Doesn't help, because you can only switch it off, due to the bug above you can't switch it on
	// Manual states wrongly on page 77 that MIDI Param Receive is ignored when received, but that is not entirely true 

	struct gOB6GlobalSettings {
		std::vector<DSIGlobalSettingDefinition> definitions = {
			{ TRANSPOSE, 1025, { "Transpose", "Tuning", 12, -12, 12 },  -12 }, // Default 12, displayed as 0
			{ MASTER_TUNE, 1024, { "Master Tune", "Tuning", 25, -50, 50 }, -50 }, // Default 50, displayed as 0
			{ MIDI_CHANNEL, 1026, { "MIDI Channel", "MIDI", 1, { {0, "Omni"}, {1, "1" }, {2, "2" }, {3, "3" }, {4, "4" }, {5, "5" }, {6, "6" }, {7, "7" }, {8, "8" }, {9, "9" }, {10, "10" }, {11, "11" }, {12, "12" }, {13, "13" }, {14, "14" }, {15, "15" }, {16, "16" }, {17, "MPE"}} } },
			{ MIDI_CLOCK, 1027, { "MIDI Clock Mode", "MIDI", 1, { {0, "Off"}, { 1, "Master" }, { 2, "Slave" }, { 3, "Slave Thru" }, { 4, "Slave No S/S"} } } },
			{ CLOCK_PORT, 1028, { "Clock Port", "MIDI", 0, { {0, "MIDI"}, { 1, "USB" } } } },
			{ PARAM_TRANSMIT, 1029, { "MIDI Param Xmit", "MIDI", 2, { {0, "Off"}, { 1, "CC" }, { 2, "NRPN"}, {3, "CC with sequencer"}, {4, "NRPN with sequencer"} } } },
			{ PARAM_RECEIVE, 1030, { "MIDI Param Rcv", "MIDI", 2, { {0, "Off"}, { 1, "CC" }, { 2, "NRPN"} } } },
			{ MIDI_CONTROL, 1035, { "MIDI Control", "MIDI", true } },
			{ MIDI_SYSEX, 1032, { "MIDI SysEx", "MIDI", 0, { {0, "MIDI"}, { 1, "USB" } } } },
			{ MIDI_OUT, 1033, { "MIDI Out", "MIDI", 0, { { 0, "MIDI" }, { 1, "USB"}, { 2, "MIDI+USB" }, { 3, "Ply" } } } },
			{ ARP_BEAT_SYNC, 1036 /* undocumented */, { "Arp Beat Sync", "MIDI", false } },
			{ LOCAL_CONTROL, 1031, { "Local Control Enabled", "MIDI", true } },
			{ VELOCITY_RESPONSE, 1041, { "Velocity Response", "Keyboard", 0, 0, 7 }  },
			{ AFTERTOUCH_RESPONSE, 1042, { "Aftertouch Response", "Keyboard", 0, 0, 3 } },
			{ STEREO_MONO, 1043, { "Stereo or Mono", "Audio Setup", 0, { {0, "Stereo" }, { 1, "Mono" } } } },
			{ POT_MODE, 1037, { "Pot Mode", "Front controls", 2, { {0, "Relative"}, { 1, "Pass Thru" }, { 2, "Jump" } } } },
			{ SEQ_JACK, 1039, { "Seq jack", "Pedals", 0, { {0, "Normal"}, { 1, "Tri" }, { 2, "Gate" }, { 3, "Gate/Trigger" } } } },
			{ ALT_TUNING, 1044, { "Alternative Tuning", "Scales", 0, kDSIAlternateTunings() } },
			{ MASTER_PITCHBEND_RANGE, 1046 /* 0x416 */, { "Master PB range", "MPE", 2, 1, 96 }, +1 },
			{ VOICE_PITCHBEND_RANGE, 0x417, { "Voice PB range", "MPE", 48, 1, 96 }, +1 },
			{ Y_AXIS_DESTINATION, 0x418, { "Y-Axis Dest", "MPE", 0, { {0, "LPF"}, { 1, "PW1" }, { 2, "PW2" }, { 3, "PW12" } } } },
			{ Y_AXIS_MODE, 0x419, { "Y-Axis Mode", "MPE", 0, { {0, "Unipolar"}, { 1, "Bipolar" } } } },
			{ VINTAGE_MODE, 0x41a, { "Vintage Mode", "Controls", 0, { {0, "Off"}, { 1, "On" } } } },
		};
	};
	std::unique_ptr<gOB6GlobalSettings> sOB6GlobalSettings;
	std::vector<DSIGlobalSettingDefinition> &kOB6GlobalSettings() {
		if (!sOB6GlobalSettings) {
			sOB6GlobalSettings = std::make_unique<gOB6GlobalSettings>();
		}
		return sOB6GlobalSettings->definitions;
	}

	class GlobalSettingsFile : public DataFile {
	public:
		using DataFile::DataFile;

		std::string name() const override
		{
			return "OB6 MASTER DATA";
		}
	};

	std::vector<Range<int>> kOB6BlankOutZones = {
		{ 107, 127 }, // 20 Characters for the name
	};


	OB6::OB6() : DSISynth(0b00101110 /* OB-6 ID */)
	{
	    initGlobalSettings();

		// This was the test for the MPE mode
		//MemoryBlock memory;
		//memory.loadFromHexString("f0 01 2e 0f 0c 32 11 02 00 02 02 01 00 00 00 00 02 00 00 00 00 00 00 00 01 2f 00 01 01 f7");
		//MidiMessage syx(memory.getData(), (int) memory.getSize());
		//jassert(channelIfValidDeviceResponse(syx).isValid());
	}

	std::string OB6::getName() const
	{
		return "DSI OB-6";
	}

	int OB6::numberOfBanks() const
	{
		return 10;
	}

	int OB6::numberOfPatches() const
	{
		return 100;
	}

	std::string OB6::friendlyProgramName(MidiProgramNumber programNo) const
	{
		return (boost::format("#%03d") % programNo.toZeroBasedWithBank()).str();
	}

	std::string OB6::friendlyBankName(MidiBankNumber bankNo) const
	{
		return (boost::format("%03d - %03d") % (bankNo.toZeroBased() * numberOfPatches()) % (bankNo.toOneBased() * numberOfPatches() - 1)).str();
	}

	std::shared_ptr<DataFile> OB6::patchFromSysex(const std::vector<MidiMessage>& messages) const
	{
		if (messages.size() == 1 && isOwnSysex(messages[0])) {
			auto& message = messages[0];
			if (message.getSysExDataSize() > 2) {
				uint8 messageCode = message.getSysExData()[2];
				if (messageCode == 0x02 /* program data dump */ || messageCode == 0x03 /* edit buffer dump */) {
					int startIndex = messageCode == 0x02 ? 5 : 3;
					const uint8 *startOfData = &message.getSysExData()[startIndex];
					auto globalDumpData = unescapeSysex(startOfData, message.getSysExDataSize() - startIndex, 1024);
					MidiProgramNumber place;
					if (messageCode == 0x02) {
						place = MidiProgramNumber::fromZeroBaseWithBank(MidiBankNumber::fromZeroBase(message.getSysExData()[3], 100),  message.getSysExData()[4]);
					}
					auto patch = std::make_shared<OB6Patch>(OB6::PATCH, globalDumpData, place);
					return patch;
				}
			}
		}
		return std::shared_ptr<Patch>();
	}

	std::shared_ptr<DataFile> OB6::patchFromPatchData(const Synth::PatchData &data, MidiProgramNumber place) const
	{
		auto patch = std::make_shared<OB6Patch>(OB6::PATCH, data, place);
		return patch;
	}

	Synth::PatchData OB6::filterVoiceRelevantData(std::shared_ptr<DataFile> unfilteredData) const
	{
		return Patch::blankOut(kOB6BlankOutZones, unfilteredData->data());
	}

	std::vector<juce::MidiMessage> OB6::patchToSysex(std::shared_ptr<DataFile> patch) const
	{
		std::vector<uint8> message({ 0x01 /* DSI */, midiModelID_, 0x03 /* Edit Buffer data */ });
		auto escaped = escapeSysex(patch->data(), patch->data().size());
		std::copy(escaped.begin(), escaped.end(), std::back_inserter(message));
		return std::vector<juce::MidiMessage>({ MidiHelpers::sysexMessage(message) });
	}

	std::vector<juce::MidiMessage> OB6::deviceDetect(int channel)
	{
		ignoreUnused(channel);
		return { requestGlobalSettingsDump() };
	}

	MidiChannel OB6::channelIfValidDeviceResponse(const MidiMessage &message)
	{
		if (isGlobalSettingsDump(message)) {
			localControl_ = message.getSysExData()[3 + LOCAL_CONTROL] == 1;
			midiControl_ = message.getSysExData()[3 + MIDI_CONTROL] == 1;
			// Can you use this to init the global settings!
			auto dataFile = loadData({ message }, GLOBAL_SETTINGS);
			if (dataFile.size() > 0) {
				setGlobalSettingsFromDataFile(dataFile[0]);
			}

			int midiChannel = message.getSysExData()[MIDI_CHANNEL + 3];
			if (midiChannel == 0) {
				return MidiChannel::omniChannel();
			}
			if (midiChannel == 17) {
				// Now that is very Sequential, make sure this is a valid channel as well!
				// To noe, the OB-6's 6 voices operate on channels 2-7 in MPE mode.
				return MidiChannel::MPEmode(1);
			}
			return  MidiChannel::fromOneBase(midiChannel);
		}
		return MidiChannel::invalidChannel();
	}

	void OB6::changeInputChannel(MidiController *controller, MidiChannel newChannel, std::function<void()> onFinished)
	{
		// The OB6 will change its channel with a nice NRPN message
		// See page 79 of the manual
		controller->getMidiOutput(midiOutput())->sendBlockOfMessagesFullSpeed(createNRPN(1026, newChannel.toOneBasedInt()));
		setCurrentChannelZeroBased(midiInput(), midiOutput(), newChannel.toZeroBasedInt());
		onFinished();
	}

	void OB6::setMidiControl(MidiController *controller, bool isOn)
	{
		// See page 77 of the manual
		controller->getMidiOutput(midiOutput())->sendBlockOfMessagesFullSpeed(createNRPN(1031, isOn ? 1 : 0));
		midiControl_ = isOn;
	}

	MidiNote OB6::getLowestKey() const
	{
		return MidiNote(0x24);
	}

	MidiNote OB6::getHighestKey() const
	{
		return MidiNote(0x60 - 12);
	}

	void OB6::changeOutputChannel(MidiController *controller, MidiChannel channel, std::function<void()> onFinished)
	{
		// The OB6 has no split output and input MIDI channels, so we must take care with the MIDI routing. Don't do that now
		changeInputChannel(controller, channel, onFinished);
	}

	void OB6::setLocalControl(MidiController *controller, bool localControlOn)
	{
		// This is the documented way, but at least my OB6 completely ignores it
		controller->getMidiOutput(midiOutput())->sendBlockOfMessagesFullSpeed(createNRPN(1035, localControlOn ? 1 : 0));
		// DSI support recommended to use the CC parameter, and that funnily works - but only, if MIDI control is turned on (makes sense)
		// Interestingly, this works even when the "Param Rcv" is set to NRPN. The documentation suggestions otherwise.
		controller->getMidiOutput(midiOutput())->sendMessageNow(MidiMessage::controllerEvent(channel().toOneBasedInt(), 0x7a, localControlOn ? 1 : 0));
		localControl_ = localControlOn;
	}

	std::vector<juce::MidiMessage> OB6::requestDataItem(int itemNo, int dataTypeID)
	{
		switch (dataTypeID) {
		case PATCH:
			return { requestEditBufferDump() }; // Interpreting this as the edit buffer
		case GLOBAL_SETTINGS:
			return { requestGlobalSettingsDump() };
		case ALTERNATE_TUNING:
			return { MidiTuning::createTuningDumpRequest(0x01, MidiProgramNumber::fromZeroBase(itemNo)) };
		default:
			jassert(false);
		}
		return {};
	}

	int OB6::numberOfDataItemsPerType(int dataTypeID) const
	{
		switch (dataTypeID)
		{
		case PATCH: return 1;
		case GLOBAL_SETTINGS: return 1;
		case ALTERNATE_TUNING: return 17;
		default: return 0;
		}
	}

	bool OB6::isDataFile(const MidiMessage &message, int dataTypeID) const
	{
		if (isOwnSysex(message)) {
			switch (dataTypeID)
			{
			case PATCH:
				return isEditBufferDump({ message });
			case GLOBAL_SETTINGS:
				return isGlobalSettingsDump(message);
			case ALTERNATE_TUNING:
				return MidiTuning::isTuningDump(message);
			default:
				jassert(false);
			}
		}
		return false;
	}

	std::vector<std::shared_ptr<midikraft::DataFile>> OB6::loadData(std::vector<MidiMessage> messages, int dataTypeID) const
	{
		std::vector<std::shared_ptr<DataFile>> result;
		for (auto m : messages) {
			if (isDataFile(m, dataTypeID)) {
				switch (dataTypeID) {
				case GLOBAL_SETTINGS: {
					std::vector<uint8> syx(m.getSysExData(), m.getSysExData() + m.getSysExDataSize());
					auto storage = std::make_shared<GlobalSettingsFile>(GLOBAL_SETTINGS, syx);
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
					// Not implemented yet
				}
			}
		}
		return result;
	}

	std::vector<midikraft::DataFileLoadCapability::DataFileDescription> OB6::dataTypeNames() const
	{
		return { { "Patch", true, true}, { "Global Settings", true, false}, { "Alternate Tuning", false, true } };
	}

	MidiMessage OB6::requestGlobalSettingsDump() const {
		return MidiHelpers::sysexMessage({ 0x01 /* DSI */, midiModelID_, 0x0e /* Global parameter transmit */ });
	}

	bool OB6::isGlobalSettingsDump(MidiMessage const &message) const {
		return isOwnSysex(message) && message.getSysExDataSize() > 2 && message.getSysExData()[2] == 0x0f /* main parameter data */;
	}

	void OB6::initGlobalSettings()
	{
		// Loop over it and fill out the GlobalSettings Properties
		globalSettings_.clear();
		for (size_t i = 0; i < kOB6GlobalSettings().size(); i++) {
			auto setting = std::make_shared<TypedNamedValue>(kOB6GlobalSettings()[i].typedNamedValue);
			globalSettings_.push_back(setting);
		}
		globalSettingsTree_ = ValueTree("OB6SETTINGS");
		globalSettings_.addToValueTree(globalSettingsTree_);
		globalSettingsTree_.addListener(&updateSynthWithGlobalSettingsListener_);
	}

	midikraft::DataFileLoadCapability * OB6::loader()
	{
		//TODO this could be standard for all DSISynths
		return this;
	}

	int OB6::settingsDataFileType() const
	{
		//TODO this could be standard for all DSISynths
		return GLOBAL_SETTINGS;
	}

	std::vector<midikraft::DSIGlobalSettingDefinition> OB6::dsiGlobalSettings() const
	{
		return kOB6GlobalSettings();
	}

	std::shared_ptr<DataFile> OB6::patchFromProgramDumpSysex(const std::vector<MidiMessage>& message) const
	{
		return patchFromSysex(message);
	}

	std::vector<juce::MidiMessage> OB6::patchToProgramDumpSysex(std::shared_ptr<DataFile> patch, MidiProgramNumber programNumber) const
	{
		// Create a program data dump message
		int programPlace = programNumber.toZeroBasedWithBank();
		std::vector<uint8> programDataDump({ 0x01 /* DSI */, midiModelID_, 0x02 /* Program Data */, (uint8)(programPlace / numberOfPatches()), (uint8)(programPlace % numberOfPatches()) });
		auto escaped = escapeSysex(patch->data(), patch->data().size());
		std::copy(escaped.begin(), escaped.end(), std::back_inserter(programDataDump));
		return std::vector<MidiMessage>({ MidiHelpers::sysexMessage(programDataDump) });
	}

}
