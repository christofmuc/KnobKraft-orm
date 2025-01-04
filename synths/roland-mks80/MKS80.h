/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "JuceHeader.h"

#include "Synth.h"
#include "HasBanksCapability.h"
#include "EditBufferCapability.h"
#include "HandshakeLoadingCapability.h"
#include "DetailedParametersCapability.h"
#include "SoundExpanderCapability.h"
#include "LegacyLoaderCapability.h"
#include "SimpleDiscoverableDevice.h"
#include "SupportedByBCR2000.h"

#include "MKS80_Parameter.h"

namespace midikraft {

	class MKS80 : public Synth, public EditBufferCapability, public HasBanksCapability, public HandshakeLoadingCapability,
		public SimpleDiscoverableDevice, public DetailedParametersCapability,
		public LegacyLoaderCapability, public SoundExpanderCapability, public SupportedByBCR2000 {
	public:
		MKS80();
		virtual std::string friendlyProgramName(MidiProgramNumber programNo) const override;
		virtual std::shared_ptr<DataFile> patchFromPatchData(const Synth::PatchData& data, MidiProgramNumber place) const override;
		virtual bool isOwnSysex(MidiMessage const& message) const override;

		// HasBanksCapability
		virtual int numberOfBanks() const override;
		virtual int numberOfPatches() const override;
		virtual std::string friendlyBankName(MidiBankNumber bankNo) const override;
		virtual std::vector<juce::MidiMessage> bankSelectMessages(MidiBankNumber bankNo) const override;

		virtual std::vector<MidiMessage> deviceDetect(int channel) override;
		virtual int deviceDetectSleepMS() override;
		virtual MidiChannel channelIfValidDeviceResponse(const MidiMessage &message) override;
		virtual bool needsChannelSpecificDetection() override;
		virtual bool endDeviceDetect(MidiMessage &endMessageOut) const override;

		virtual std::string getName() const override;

		// Edit Buffer Capability
		virtual std::vector<MidiMessage> requestEditBufferDump() const override;
		virtual bool isEditBufferDump(const std::vector<MidiMessage>& message) const override;
		virtual std::shared_ptr<DataFile> patchFromSysex(const std::vector<MidiMessage>& message) const override;
		virtual std::vector<MidiMessage> patchToSysex(std::shared_ptr<DataFile> patch) const override;
		virtual MidiMessage saveEditBufferToProgram(int programNumber) override;

		// HandshakeLoadingCapability
		virtual std::shared_ptr<ProtocolState> createStateObject() override;
		virtual void startDownload(std::shared_ptr<SafeMidiOutput> output, std::shared_ptr<ProtocolState> saveState) override;
		virtual bool isNextMessage(MidiMessage const &message, std::vector<MidiMessage> &answer, std::shared_ptr<ProtocolState> state) override;

		// DetailedParametersLoadingCapability
		virtual std::vector<std::shared_ptr<SynthParameterDefinition>> allParameterDefinitions() const override;

		// Sound expander capability
		virtual bool canChangeInputChannel() const override;
		virtual void changeInputChannel(MidiController *controller, MidiChannel channel, std::function<void()> onFinished) override;
		virtual MidiChannel getInputChannel() const override;
		virtual bool hasMidiControl() const override;
		virtual bool isMidiControlOn() const override;
		virtual void setMidiControl(MidiController *controller, bool isOn) override;

		// Override for funky formats
		TPatchVector loadSysex(std::vector<MidiMessage> const &sysexMessages) override;

		// LegacyLoaderCapability
		virtual std::string additionalFileExtensions() override;
		virtual bool supportsExtension(std::string const &filename) override;
		virtual TPatchVector load(std::string const &filename, std::vector<uint8> const &fileContent) override;
		
		// SupportedByBCR2000
		virtual void setupBCR2000(BCR2000 &bcr) override;
		virtual void setupBCR2000View(BCR2000Proxy *view, TypedNamedValueSet &parameterModel, ValueTree &valueTree) override;

		// Other methods to help with the complexity of the MKS80 format
		static TPatchVector patchesFromAPRs(std::vector<std::vector<uint8>> const &toneData, std::vector<std::vector<uint8>> const &patchData);

	private:
		std::string presetName();

		static uint8 rolandChecksum(std::vector<uint8>::iterator start, std::vector<uint8>::iterator end);

		MidiMessage buildHandshakingMessage(MKS80_Operation_Code code) const;
		MidiMessage buildHandshakingMessage(MKS80_Operation_Code code, MidiChannel channel) const;

		MKS80_Operation_Code getSysexOperationCode(MidiMessage const &message) const;
	};

}