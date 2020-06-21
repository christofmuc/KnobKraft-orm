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
		MASTER_TUNE,
		MIDI_CHANNEL,
		MIDI_CLOCK,
		CLOCK_PORT,
		PARAM_TRANSMIT,
		PARAM_RECEIVE,
		MIDI_CONTROL,
		MIDI_SYSEX,
		MIDI_OUT,
		LOCAL_CONTROL,
		SEQ_JACK,
		POT_MODE,
		SUSTAIN_POLARITY,
		ALT_TUNING,
		VELOCITY_RESPONSE,
		AFTERTOUCH_RESPONSE,
		STEREO_MONO,
		ARP_BEAT_SYNC
	};

	struct OB6GlobalSettingDefinition {
		int sysexIndex;
		int nrpn;
		TypedNamedValue typedNamedValue;
		int displayOffset = 0;
	};

	// Warnings for the user
	// 
	// The panel will only work when the parameter "MIDI Param Rcv" is set to NRPN. And if you switch it away, it will stop working.
	// Same with MIDI Control Off - the synth will no longer respond to the NRPN messages we send.
	// Also, the MIDI sysex switch must be set to USB if we talk to the synth via USB

	// Bugs in the OB6 Sysex implementation (V 1.5.8):
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

	std::vector<OB6GlobalSettingDefinition> kOB6GlobalSettings = {
		{ TRANSPOSE, 1025, { "Transpose", "Tuning", 12, -12, 12 },  -12 }, // Default 12, displayed as 0
		{ MASTER_TUNE, 1024, { "Master Tune", "Tuning", 25, -50, 50 }, -50 }, // Default 50, displayed as 0
		{ MIDI_CHANNEL, 1026, { "MIDI Channel", "MIDI", 1, { {0, "Omni"}, {1, "1" }, {2, "2" }, {3, "3" }, {4, "4" }, {5, "5" }, {6, "6" }, {7, "7" }, {8, "8" }, {9, "9" }, {10, "10" }, {11, "11" }, {12, "12" }, {13, "13" }, {14, "14" }, {15, "15" }, {16, "16" }} } },
		{ MIDI_CLOCK, 1027, { "MIDI Clock Mode", "MIDI", 1, { {0, "Off"}, { 1, "Master" }, { 2, "Slave" }, { 3, "Slave Thru" }, { 4, "Slave No S/S"} } } },
		{ CLOCK_PORT, 1028, { "Clock Port", "MIDI", 0, { {0, "MIDI"}, { 1, "USB" } } } },
		{ PARAM_TRANSMIT, 1029, { "MIDI Param Xmit", "MIDI", 2, { {0, "Off"}, { 1, "CC" }, { 2, "NRPN"}, {3, "CC with sequencer"}, {4, "NRPN with sequencer"} } } },
		{ PARAM_RECEIVE, 1030, { "MIDI Param Rcv", "MIDI", 2, { {0, "Off"}, { 1, "CC" }, { 2, "NRPN"} } } },
		{ MIDI_CONTROL, 1031, { "MIDI Control", "MIDI", true } },
		{ MIDI_SYSEX, 1032, { "MIDI SysEx", "MIDI", 0, { {0, "MIDI"}, { 1, "USB" } } } },
		{ MIDI_OUT, 1033, { "MIDI Out", "MIDI", 0, { { 0, "MIDI" }, { 1, "USB"}, { 2, "MIDI+USB" }, { 3, "Ply" } } } },
		//{ ARP_BEAT_SYNC, 1036 /* undocumented */, { "Arp Beat Sync", "MIDI", Value(), ValueType::Lookup, 0, 1, { {0, "Off"}, { 1, "Quantize" } } } },
		{ LOCAL_CONTROL, 1035, { "Local Control Enabled", "MIDI", true } },
		{ VELOCITY_RESPONSE, 1041, { "Velocity Response", "Keyboard", 0, 0, 7 }  },
		{ AFTERTOUCH_RESPONSE, 1042, { "Aftertouch Response", "Keyboard", 0, 0, 3 } },
		{ STEREO_MONO, 1043, { "Stereo or Mono", "Audio Setup", 0, { {0, "Stereo" }, { 1, "Mono" } } } },
		{ POT_MODE, 1037, { "Pot Mode", "Front controls", 2, { {0, "Relative"}, { 1, "Pass Thru" }, { 2, "Jump" } } } },
		{ SEQ_JACK, 1039, { "Seq jack", "Pedals", 0, { {0, "Normal"}, { 1, "Tri" }, { 2, "Gate" }, { 3, "Gate/Trigger" } } } },
		{ ALT_TUNING, 1044, { "Alternative Tuning", "Scales", 0, kDSIAlternateTunings() } },
		{ SUSTAIN_POLARITY, 1040, { "Sustain polarity", "Controls", 0, { {0, "Normal"}, { 1, "Reversed" }, { 2, "n-r" }, { 3, "r-n" } } } },
	};

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


	OB6::OB6() : DSISynth(0b00101110 /* OB-6 ID */), updateSynthWithGlobalSettingsListener_(this)
	{
		initGlobalSettings();
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

	std::string OB6::friendlyBankName(MidiBankNumber bankNo) const
	{
		return (boost::format("%03d - %03d") % (bankNo.toZeroBased() * numberOfPatches()) % (bankNo.toOneBased() * numberOfPatches() - 1)).str();
	}

	std::shared_ptr<Patch> OB6::patchFromSysex(const MidiMessage& message) const
	{
		if (isOwnSysex(message)) {
			if (message.getSysExDataSize() > 2) {
				uint8 messageCode = message.getSysExData()[2];
				if (messageCode == 0x02 /* program data dump */ || messageCode == 0x03 /* edit buffer dump */) {
					int startIndex = messageCode == 0x02 ? 5 : 3;
					const uint8 *startOfData = &message.getSysExData()[startIndex];
					auto globalDumpData = unescapeSysex(startOfData, message.getSysExDataSize() - startIndex, 1024);
					auto patch = std::make_shared<OB6Patch>(OB6::PATCH, globalDumpData);
					if (messageCode == 0x02) {
						patch->setPatchNumber(MidiProgramNumber::fromZeroBase(message.getSysExData()[3] * 100 + message.getSysExData()[4]));
					}
					return patch;
				}
			}
		}
		return std::shared_ptr<Patch>();
	}

	std::shared_ptr<DataFile> OB6::patchFromPatchData(const Synth::PatchData &data, MidiProgramNumber place) const
	{
		auto patch = std::make_shared<OB6Patch>(OB6::PATCH, data);
		patch->setPatchNumber(place);
		return patch;
	}

	Synth::PatchData OB6::filterVoiceRelevantData(std::shared_ptr<DataFile> unfilteredData) const
	{
		return Patch::blankOut(kOB6BlankOutZones, unfilteredData->data());
	}

	std::vector<juce::MidiMessage> OB6::patchToSysex(const Patch &patch) const
	{
		std::vector<uint8> message({ 0x01 /* DSI */, midiModelID_, 0x03 /* Edit Buffer data */ });
		auto escaped = escapeSysex(patch.data(), patch.data().size());
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
			int midiChannel = message.getSysExData()[MIDI_CHANNEL + 3];
			if (midiChannel == 0) {
				return MidiChannel::omniChannel();

			}
			// Can you use this to init the global settings!
			auto dataFile = loadData({ message }, GLOBAL_SETTINGS);
			if (dataFile.size() > 0) {
				setGlobalSettingsFromDataFile(dataFile[0]);
			}

			return  MidiChannel::fromOneBase(midiChannel);
		}
		return MidiChannel::invalidChannel();
	}

	void OB6::changeInputChannel(MidiController *controller, MidiChannel newChannel, std::function<void()> onFinished)
	{
		// The OB6 will change its channel with a nice NRPN message
		// See page 79 of the manual
		controller->getMidiOutput(midiOutput())->sendBlockOfMessagesNow(createNRPN(1026, newChannel.toOneBasedInt()));
		setCurrentChannelZeroBased(midiInput(), midiOutput(), newChannel.toZeroBasedInt());
		onFinished();
	}

	void OB6::setMidiControl(MidiController *controller, bool isOn)
	{
		// See page 77 of the manual
		controller->getMidiOutput(midiOutput())->sendBlockOfMessagesNow(createNRPN(1031, isOn ? 1 : 0));
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
		//controller->getMidiOutput(midiOutput())->sendBlockOfMessagesNow(createNRPN(1035, localControlOn ? 1 : 0));
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
				return isEditBufferDump(message);
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

	void OB6::setGlobalSettingsFromDataFile(std::shared_ptr<DataFile> dataFile)
	{
		if (dataFile && dataFile->dataTypeID() == GLOBAL_SETTINGS) {
			auto message = MidiMessage::createSysExMessage(dataFile->data().data(), (int)dataFile->data().size());
			if (isGlobalSettingsDump(message)) {
				std::vector<uint8> globalParameterData(&message.getSysExData()[3], message.getSysExData() + message.getSysExDataSize());
				if (globalParameterData.size() == 19) {
					// Loop over it and fill out the GlobalSettings Properties
					for (size_t i = 0; i < kOB6GlobalSettings.size(); i++) {
						if (i < globalParameterData.size()) {
							// As this is coming from a datafile, we assume this is coming from the synth (we don't store the global settings data files on the computer)
							// Therefore, don't notify the update synth listener, because that would send out the same data back to the synth where it is coming from
							globalSettingsTree_.setPropertyExcludingListener(&updateSynthWithGlobalSettingsListener_,
								Identifier(kOB6GlobalSettings[i].typedNamedValue.name()),
								var(globalParameterData[kOB6GlobalSettings[i].sysexIndex] + kOB6GlobalSettings[i].displayOffset),
								nullptr);
						}
					}
				}
				else {
					SimpleLogger::instance()->postMessage("Invalid size of global settings data - maybe an old OS version? Update the OB6 to 1.5.8!");
				}
			}
		}
	}

	//TODO needs to be merged with Rev2 code
	void OB6::GlobalSettingsListener::valueTreePropertyChanged(ValueTree& treeWhosePropertyHasChanged, const Identifier& property)
	{
		if (!ob6_->channel().isValid()) return;

		Value value = treeWhosePropertyHasChanged.getPropertyAsValue(property, nullptr, false);
		// Need to find definition for this setting now, suboptimal data structures
		for (const auto& def : kOB6GlobalSettings) {
			if (def.typedNamedValue.name() == property.getCharPointer()) {
				int newMidiValue = ((int)value.getValue()) - def.displayOffset;
				auto messages = ob6_->createNRPN(def.nrpn, newMidiValue);
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
				MidiController::instance()->getMidiOutput(ob6_->midiOutput())->sendBlockOfMessagesNow(messages);
				return;
			}
		}
	}

	void OB6::initGlobalSettings()
	{
		// Loop over it and fill out the GlobalSettings Properties
		globalSettings_.clear();
		for (size_t i = 0; i < kOB6GlobalSettings.size(); i++) {
			auto setting = std::make_shared<TypedNamedValue>(kOB6GlobalSettings[i].typedNamedValue);
			globalSettings_.push_back(setting);
		}
		globalSettingsTree_ = ValueTree("OB6SETTINGS");
		globalSettings_.addToValueTree(globalSettingsTree_);
		globalSettingsTree_.addListener(&updateSynthWithGlobalSettingsListener_);
	}

	std::vector<std::shared_ptr<TypedNamedValue>> OB6::getGlobalSettings()
	{
		return globalSettings_;
	}

	midikraft::DataFileLoadCapability * OB6::loader()
	{
		return this;
	}

	int OB6::settingsDataFileType() const
	{
		return GLOBAL_SETTINGS;
	}

	std::shared_ptr<Patch> OB6::patchFromProgramDumpSysex(const MidiMessage& message) const
	{
		return patchFromSysex(message);
	}

	std::vector<juce::MidiMessage> OB6::patchToProgramDumpSysex(const Patch &patch) const
	{
		return patchToSysex(patch);
	}

}
