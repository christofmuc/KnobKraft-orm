/*
   Copyright (c) 2021 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "JuceHeader.h"

#include "Synth.h"
#include "HasBanksCapability.h"
#include "EditBufferCapability.h"
#include "HandshakeLoadingCapability.h"
#include "DetailedParametersCapability.h"

namespace midikraft {

	class MKS50 : public Synth, public HasBanksCapability, public EditBufferCapability, public HandshakeLoadingCapability,
		public SimpleDiscoverableDevice, public DetailedParametersCapability {
	public:
		MKS50();

		// HasBanksCapability
		virtual int numberOfBanks() const override;
		virtual int numberOfPatches() const override;
		virtual std::string friendlyBankName(MidiBankNumber bankNo) const override;

		virtual std::shared_ptr<DataFile> patchFromPatchData(const Synth::PatchData& data, MidiProgramNumber place) const override;

		virtual bool isOwnSysex(MidiMessage const& message) const override;

		virtual std::vector<MidiMessage> deviceDetect(int channel) override;
		virtual int deviceDetectSleepMS() override;
		virtual MidiChannel channelIfValidDeviceResponse(const MidiMessage& message) override;
		virtual bool needsChannelSpecificDetection() override;

		virtual std::string getName() const override;

		// Edit Buffer Capability
		virtual std::vector <MidiMessage> requestEditBufferDump() const override;
		virtual bool isEditBufferDump(const std::vector<MidiMessage>& message) const override;
		virtual std::shared_ptr<DataFile> patchFromSysex(const std::vector<MidiMessage>& message) const override;
		virtual std::vector<MidiMessage> patchToSysex(std::shared_ptr<DataFile> patch) const override;
		virtual MidiMessage saveEditBufferToProgram(int programNumber) override;

		// HandshakeLoadingCapability
		virtual std::shared_ptr<ProtocolState> createStateObject() override;
		virtual void startDownload(std::shared_ptr<SafeMidiOutput> output, std::shared_ptr<ProtocolState> saveState) override;
		virtual bool isNextMessage(MidiMessage const& message, std::vector<MidiMessage>& answer, std::shared_ptr<ProtocolState> state) override;

		// Override for funky formats
		TPatchVector loadSysex(std::vector<MidiMessage> const& sysexMessages) override;

		// DetailedParametersCapability
		std::vector<std::shared_ptr<SynthParameterDefinition>> allParameterDefinitions() const override;

	private:
		enum class MKS50_Operation_Code;

		MidiMessage buildHandshakingMessage(MKS50_Operation_Code code) const;
		MKS50::MKS50_Operation_Code getSysexOperationCode(MidiMessage const& message) const;

		MidiChannel channel_;
	};

}
