/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "Synth.h"
#include "RefaceDXDiscovery.h"
#include "EditBufferCapability.h"
#include "StreamLoadCapability.h"
#include "MasterkeyboardCapability.h"
#include "SoundExpanderCapability.h"

namespace midikraft {

	class RefaceDX : public Synth, public RefaceDXDiscovery, public EditBufferCapability, public StreamLoadCapability, public SoundExpanderCapability, public MasterkeyboardCapability {
	public:
		virtual std::string getName() const override;

		// Basic Synth
		virtual bool isOwnSysex(MidiMessage const &message) const override;
		virtual std::shared_ptr<DataFile> patchFromPatchData(const Synth::PatchData &data, MidiProgramNumber place) const override;
		virtual int numberOfBanks() const override;
		virtual int numberOfPatches() const override;
		virtual std::string friendlyBankName(MidiBankNumber bankNo) const override;
		Synth::PatchData filterVoiceRelevantData(Synth::PatchData const &unfilteredData) const;

		// Edit Buffer Capability
		virtual MidiMessage requestEditBufferDump() override;
		virtual bool isEditBufferDump(const MidiMessage& message) const override;
		virtual std::shared_ptr<Patch> patchFromSysex(const MidiMessage& message) const override;
		virtual std::vector<MidiMessage> patchToSysex(const Patch &patch) const override;
		virtual MidiMessage saveEditBufferToProgram(int programNumber) override;

		// StreamLoadCapability
		virtual bool isMessagePartOfStream(MidiMessage const &message) override;
		virtual bool isStreamComplete(std::vector<MidiMessage> const &messages) override;
		virtual bool shouldStreamAdvance(std::vector<MidiMessage> const &messages) override;

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

		// This needs to be overridden because the base implementation assumes a patch fits into a single MidiMessage, which is not true for the Reface DX
		virtual TPatchVector loadSysex(std::vector<MidiMessage> const &sysexMessages) override;

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

		juce::MidiMessage buildRequest(uint8 addressHigh, uint8 addressMid, uint8 addressLow);
		juce::MidiMessage buildParameterChange(uint8 addressHigh, uint8 addressMid, uint8 addressLow, uint8 value);
		MidiMessage buildDataBlockMessage(TDataBlock const &block) const;
		bool dataBlockFromDump(const MidiMessage &message, TDataBlock &outBlock) const;

		MidiChannel transmitChannel_ = MidiChannel::invalidChannel(); // The RefaceDX has separate send and receive channels!
		bool localControl_ = false; // And it can also turn local control on and off
		bool midiControlOn_ = false; // Can it be controlled via MIDI?
	};

}
