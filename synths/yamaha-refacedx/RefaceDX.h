/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "Synth.h"
#include "RefaceDXDiscovery.h"
#include "HasBanksCapability.h"
#include "StreamLoadCapability.h"
#include "DataFileSendCapability.h"
#include "MasterkeyboardCapability.h"
#include "SoundExpanderCapability.h"

namespace midikraft {

	class RefaceDX : public Synth, public RefaceDXDiscovery, public HasBanksCapability, public StreamLoadCapability, public DataFileSendCapability, public SoundExpanderCapability, public MasterkeyboardCapability {
	public:
		virtual std::string getName() const override;

		// Basic Synth
		virtual bool isOwnSysex(MidiMessage const &message) const override;
		virtual std::shared_ptr<DataFile> patchFromPatchData(const Synth::PatchData &data, MidiProgramNumber place) const override;
		virtual std::string friendlyProgramName(MidiProgramNumber programNo) const override;
		Synth::PatchData filterVoiceRelevantData(std::shared_ptr<DataFile> unfilteredData) const override;

		// HasBanksCapability
		virtual std::string friendlyBankName(MidiBankNumber bankNo) const override;
		virtual int numberOfBanks() const override;
		virtual int numberOfPatches() const override;

		// StreamLoadCapability
		virtual std::vector<MidiMessage> requestStreamElement(int elemNo, StreamType streamType) const override;
		virtual int numberOfStreamMessagesExpected(StreamType streamType) const override;
		virtual bool isMessagePartOfStream(MidiMessage const &message, StreamType streamType)  const override;
		virtual bool isStreamComplete(std::vector<MidiMessage> const &messages, StreamType streamType)  const override;
		virtual bool shouldStreamAdvance(std::vector<MidiMessage> const &messages, StreamType streamType)  const override;
		virtual TPatchVector loadPatchesFromStream(std::vector<MidiMessage> const &streamDump) const override;

		// Masterkeyboard Capability
		virtual void changeOutputChannel(MidiController *controller, MidiChannel newChannel, std::function<void()> finished) override;
		virtual MidiChannel getOutputChannel() const override;
		virtual bool hasLocalControl() const override;
		virtual void setLocalControl(MidiController *controller, bool localControlOn) override;
		virtual bool getLocalControl() const override;

		// SoundExpanderCapability
		virtual bool canChangeInputChannel() const override;
		virtual void changeInputChannel(MidiController *controller, MidiChannel channel, std::function<void()> finished) override;
		virtual MidiChannel getInputChannel() const override;
		virtual bool hasMidiControl() const override;
		virtual bool isMidiControlOn() const override;
		virtual void setMidiControl(MidiController *controller, bool isOn) override;

		// This is overridden from the official implementation in the RefaceDXDiscovery, because that will not tell us the MIDI channel
		virtual std::vector<juce::MidiMessage> deviceDetect(int channel) override;
		virtual MidiChannel channelIfValidDeviceResponse(const MidiMessage &message) override;

		// DataFileSendCapability
		std::vector<MidiMessage> dataFileToMessages(std::shared_ptr<DataFile> dataFile, std::shared_ptr<SendTarget> target) const override;

		// Internal
		std::vector<MidiMessage> patchToSysex(std::shared_ptr<DataFile> patch) const;

	private:
		struct TDataBlock {
			TDataBlock() = default;
			TDataBlock(uint8 hi, uint8 mid, uint8 low, std::vector<uint8>::const_iterator d, size_t bytes) :
				addressHigh(hi), addressMid(mid), addressLow(low) {
				std::copy(d, d + bytes, std::back_inserter(data));
			}
			uint8 addressHigh, addressMid, addressLow;
			std::vector<uint8> data;
		};

		juce::MidiMessage buildRequest(uint8 addressHigh, uint8 addressMid, uint8 addressLow) const;
		juce::MidiMessage buildParameterChange(uint8 addressHigh, uint8 addressMid, uint8 addressLow, uint8 value);
		MidiMessage buildDataBlockMessage(TDataBlock const &block) const;
		bool dataBlockFromDump(const MidiMessage &message, TDataBlock &outBlock) const;

		MidiChannel transmitChannel_ = MidiChannel::invalidChannel(); // The RefaceDX has separate send and receive channels!
		bool localControl_ = false; // And it can also turn local control on and off
		bool midiControlOn_ = false; // Can it be controlled via MIDI?
	};

}
