/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "OB6.h"

#include "OB6Patch.h"

#include "MidiHelpers.h"
#include "MidiController.h"

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

	std::vector<Range<int>> kOB6BlankOutZones = {
		{ 107, 127 }, // 20 Characters for the name
	};


	OB6::OB6() : DSISynth(0b00101110 /* OB-6 ID */)
	{
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
					int startIndex = messageCode == 0x02 ? 5 : 4;
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

	std::shared_ptr<DataFile> OB6::patchFromPatchData(const Synth::PatchData &data, std::string const &name, MidiProgramNumber place) const
	{
		ignoreUnused(name);
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

	juce::MidiMessage OB6::deviceDetect(int channel)
	{
		ignoreUnused(channel);
		return MidiHelpers::sysexMessage({ 0x01 /* DSI */, midiModelID_, 0x0e /* Global parameter transmit */ });
	}

	MidiChannel OB6::channelIfValidDeviceResponse(const MidiMessage &message)
	{
		if (isOwnSysex(message)) {
			if (message.getSysExDataSize() > 2 && message.getSysExData()[2] == 0x0f /* main parameter data */) {
				localControl_ = message.getSysExData()[3 + LOCAL_CONTROL] == 1;
				midiControl_ = message.getSysExData()[3 + MIDI_CONTROL] == 1;
				int midiChannel = message.getSysExData()[MIDI_CHANNEL + 3];
				if (midiChannel == 0) {
					return MidiChannel::omniChannel();

				}
				return  MidiChannel::fromOneBase(midiChannel);
			}
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

	std::shared_ptr<Patch> OB6::patchFromProgramDumpSysex(const MidiMessage& message) const
	{
		return patchFromSysex(message);
	}

	std::vector<juce::MidiMessage> OB6::patchToProgramDumpSysex(const Patch &patch) const
	{
		return patchToSysex(patch);
	}

}
