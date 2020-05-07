/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "Synth.h"
#include "EditBufferCapability.h"
#include "ProgramDumpCapability.h"
#include "BankDumpCapability.h"
#include "SoundExpanderCapability.h"

namespace midikraft {

	class Virus : public Synth, public SimpleDiscoverableDevice, EditBufferCapability, public ProgramDumpCabability, public BankDumpCapability, public SoundExpanderCapability {
	public:
		Virus();

		// Basic Synth implementation
		virtual std::string getName() const override;
		virtual bool isOwnSysex(MidiMessage const &message) const override;
		virtual int numberOfBanks() const override;
		virtual int numberOfPatches() const override;
		virtual std::string friendlyBankName(MidiBankNumber bankNo) const override;

		// This needs to be overridden because the Virus contains a lot of noise in the patch data that is not really relevant
		virtual PatchData filterVoiceRelevantData(std::shared_ptr<DataFile> unfilteredData) const override;

		// Edit Buffer Capability
		virtual MidiMessage requestEditBufferDump() override;
		virtual bool isEditBufferDump(const MidiMessage& message) const override;
		virtual std::shared_ptr<Patch> patchFromSysex(const MidiMessage& message) const override;
		virtual std::vector<MidiMessage> patchToSysex(const Patch &patch) const override;
		virtual MidiMessage saveEditBufferToProgram(int programNumber) override;

		// Program Dump Capability
		virtual std::vector<MidiMessage> requestPatch(int patchNo) override;
		virtual bool isSingleProgramDump(const MidiMessage& message) const override;
		virtual std::shared_ptr<Patch> patchFromProgramDumpSysex(const MidiMessage& message) const;
		virtual std::vector<MidiMessage> patchToProgramDumpSysex(const Patch &patch) const;

		// Bank Dump Capability
		virtual std::vector<MidiMessage> requestBankDump(MidiBankNumber bankNo) const override;
		virtual bool isBankDump(const MidiMessage& message) const override;
		virtual bool isBankDumpFinished(std::vector<MidiMessage> const &bankDump) const;
		virtual TPatchVector patchesFromSysexBank(const MidiMessage& message) const override;

		virtual std::shared_ptr<DataFile> patchFromPatchData(const Synth::PatchData &data, MidiProgramNumber place) const override;

		// Discoverable Device
		virtual MidiMessage deviceDetect(int channel) override;
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
