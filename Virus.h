/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "Synth.h"
#include "EditBufferCapability.h"
#include "HasBanksCapability.h"
#include "ProgramDumpCapability.h"
#include "BankDumpCapability.h"
#include "SoundExpanderCapability.h"

namespace midikraft {

	class Virus : public Synth, public SimpleDiscoverableDevice, public HasBanksCapability, public EditBufferCapability, public ProgramDumpCabability, public BankDumpCapability, public SoundExpanderCapability {
	public:
		Virus();

		// Basic Synth implementation
		virtual std::string getName() const override;
		virtual bool isOwnSysex(MidiMessage const &message) const override;
		virtual std::string friendlyProgramName(MidiProgramNumber programNo) const override;

		// HasBanksCapability
		virtual int numberOfBanks() const override;
		virtual int numberOfPatches() const override;
		virtual std::string friendlyBankName(MidiBankNumber bankNo) const override;

		// This needs to be overridden because the Virus contains a lot of noise in the patch data that is not really relevant
		virtual PatchData filterVoiceRelevantData(std::shared_ptr<DataFile> unfilteredData) const override;

		// Edit Buffer Capability
		virtual std::vector<MidiMessage> requestEditBufferDump() const override;
		virtual bool isEditBufferDump(const std::vector<MidiMessage>& message) const override;
		virtual std::shared_ptr<DataFile> patchFromSysex(const std::vector <MidiMessage>& message) const override;
		virtual std::vector<MidiMessage> patchToSysex(std::shared_ptr<DataFile> patch) const override;
		virtual MidiMessage saveEditBufferToProgram(int programNumber) override;

		// Program Dump Capability
		virtual std::vector<MidiMessage> requestPatch(int patchNo) const override;
		virtual bool isSingleProgramDump(const std::vector<MidiMessage>& message) const override;
		virtual MidiProgramNumber getProgramNumber(const std::vector<MidiMessage> &message) const override;
		virtual std::shared_ptr<DataFile> patchFromProgramDumpSysex(const std::vector <MidiMessage>& message) const override;
		virtual std::vector<MidiMessage> patchToProgramDumpSysex(std::shared_ptr<DataFile> patch, MidiProgramNumber programNumber) const override;

		// Bank Dump Capability
		virtual std::vector<MidiMessage> requestBankDump(MidiBankNumber bankNo) const override;
		virtual bool isBankDump(const MidiMessage& message) const override;
		virtual bool isBankDumpFinished(std::vector<MidiMessage> const &bankDump) const override;
		virtual TPatchVector patchesFromSysexBank(const MidiMessage& message) const override;

		virtual std::shared_ptr<DataFile> patchFromPatchData(const Synth::PatchData &data, MidiProgramNumber place) const override;

		// Discoverable Device
		virtual std::vector<juce::MidiMessage> deviceDetect(int channel) override;
		virtual int deviceDetectSleepMS() override;
		virtual MidiChannel channelIfValidDeviceResponse(const MidiMessage &message) override;
		virtual bool needsChannelSpecificDetection() override;

		// SoundExpanderCapability
		virtual bool canChangeInputChannel() const override;
		virtual void changeInputChannel(MidiController *controller, MidiChannel channel, std::function<void()> finished) override;
		virtual MidiChannel getInputChannel() const override;
		virtual bool hasMidiControl() const override;
		virtual bool isMidiControlOn() const override;
		virtual void setMidiControl(MidiController *controller, bool isOn) override;

	private:
		MidiMessage createSysexMessage(std::vector<uint8> const &message) const;
		MidiMessage createParameterChangeSingle(int page, int paramNo, uint8 value) const;
		std::vector<uint8> getPagesFromMessage(MidiMessage const &message, int dataStartIndex) const;

		uint8 deviceID_; // This is only found out after the first message from the device - how do you deal with this when you have multiple Viruses?
	};

}
