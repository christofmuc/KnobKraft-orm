/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "Synth.h"

#include "MidiController.h"
#include "Patch.h"
#include "Additive.h"
#include "DrawbarOrgan.h"
#include "SupportedByBCR2000.h"

#include "HasBanksCapability.h"
#include "ProgramDumpCapability.h"
#include "BankDumpCapability.h"
#include "ReadonlySoundExpander.h"
#include "AdditiveCapability.h"
#include "HybridWaveCapability.h"
#include "DataFileLoadCapability.h"
#include "DetailedParametersCapability.h"
#include "BidirectionalSyncCapability.h"
#include "SendsProgramChangeCapability.h"
#include "CreateInitPatchDataCapability.h"
#include "DataFileSendCapability.h"

#include "SupportedByBCR2000.h" // Should be moved into the bidirectional sync?

#include <set>

namespace midikraft {

	class KawaiK3Wave;
	class KawaiK3Parameter;
	class KawaiK3Control;
	class SynthView;

	class KawaiK3 : public Synth, public SupportedByBCR2000, public SimpleDiscoverableDevice, public HasBanksCapability, public ProgramDumpCabability, public BankDumpCapability,
		public DataFileLoadCapability, public DataFileSendCapability, public DetailedParametersCapability, public BidirectionalSyncCapability,
		public SendsProgramChangeCapability, public CreateInitPatchDataCapability,
		public ReadonlySoundExpander, public AdditiveCapability, public HybridWaveCapability {
	public:
		static const MidiProgramNumber kFakeEditBuffer;

		enum DataFileType {
			K3_PATCH = 0,
			K3_WAVE = 1
		};

		enum class WaveType {
			USER_WAVE = 100,
			USER_WAVE_CARTRIDGE = 101,
			//MIDI_WAVE = 102// This is peculiar, I still have to find out what that is? Might be mistaken in the manual, because my K3M does not respond to code 102
		};

		KawaiK3();

		// Basic Synth implementation
		virtual std::string getName() const override;
		virtual bool isOwnSysex(MidiMessage const &message) const override;
		virtual std::string friendlyProgramName(MidiProgramNumber programNo) const override;
		std::shared_ptr<DataFile> patchFromPatchData(const Synth::PatchData& data, MidiProgramNumber place) const override;

		// HasBanksCapabaility
		virtual int numberOfBanks() const override;
		virtual int numberOfPatches() const override;
		virtual std::string friendlyBankName(MidiBankNumber bankNo) const override;

		// Special Synth implementation override, because the K3 sends actually write confirmation messages in 1985. Awesome engineers!
		virtual void sendDataFileToSynth(std::shared_ptr<DataFile> dataFile, std::shared_ptr<SendTarget> target) override;
		virtual void sendBlockOfMessagesToSynth(juce::MidiDeviceInfo const& midiOutput, std::vector<MidiMessage> const& buffer) override;

		// Discoverable Device
		virtual std::vector<juce::MidiMessage> deviceDetect(int channel) override;
		virtual int deviceDetectSleepMS() override;
		virtual MidiChannel channelIfValidDeviceResponse(const MidiMessage &message) override;
		virtual bool needsChannelSpecificDetection() override;

		// Program Dump Capability
		virtual std::vector<MidiMessage> requestPatch(int patchNo) const override;
		virtual bool isSingleProgramDump(const std::vector<MidiMessage>& message) const override;
		virtual MidiProgramNumber getProgramNumber(const std::vector<MidiMessage> &message) const override;
		virtual std::shared_ptr<DataFile> patchFromProgramDumpSysex(const std::vector<MidiMessage>& message) const override;
		virtual std::vector<MidiMessage> patchToProgramDumpSysex(std::shared_ptr<DataFile> patch, MidiProgramNumber programNumber) const override;
		virtual TPatchVector patchesFromSysexBank(const MidiMessage& message) const override;

		// Bank Dump Capability
		virtual std::vector<MidiMessage> requestBankDump(MidiBankNumber bankNo) const override;
		virtual bool isBankDump(const MidiMessage& message) const override;
		virtual bool isBankDumpFinished(std::vector<MidiMessage> const &bankDump) const override;

		// SoundExpanderCapability
		virtual MidiChannel getInputChannel() const override;

		// AdditiveCapability
		virtual void selectRegistration(Patch *currentPatch, DrawbarOrgan::RegistrationDefinition selectedRegistration) override;
		virtual void selectHarmonics(Patch *currentPatch, std::string const &name, Additive::Harmonics const &selectedHarmonics) override;

		// HybridWaveCapability
		virtual std::vector<float> romWave(int waveNo) override;
		virtual std::string waveName(int waveNo) override;

		// Implementation of BCR2000 sync
		virtual void setupBCR2000(BCR2000 &bcr) override;
		virtual void setupBCR2000View(BCR2000Proxy* view, TypedNamedValueSet& parameterModel, ValueTree& valueTree) override;

		// This needs to be overridden to handle the wave dumps together with the patch dumps
		virtual TPatchVector loadSysex(std::vector<MidiMessage> const &sysexMessages) override;

		// Kawai K3 specifics
		std::shared_ptr<Patch> k3PatchFromSysex(const MidiMessage& message, int indexIntoBankDump = 0) const;
		MidiMessage k3PatchToSysex(Synth::PatchData const &patch, int programNo, bool produceWaveInsteadOfPatch) const;

		typedef std::vector<uint8> WaveData;

		MidiMessage requestWaveBufferDump(WaveType waveType) const;
		bool isWaveBufferDump(const MidiMessage& message) const;
		bool isBankDumpAndNotWaveDump(const MidiMessage& message) const;
		std::shared_ptr<KawaiK3Wave> waveFromSysex(const MidiMessage& message) const;
		MidiMessage waveToSysex(std::shared_ptr<KawaiK3Wave> wave);

		bool isWriteConfirmation(MidiMessage const &message);

		// DataFileLoadCapability
		virtual std::vector<MidiMessage> requestDataItem(int itemNo, int dataTypeID) override;
		virtual int numberOfDataItemsPerType(int dataTypeID) const override;
		virtual bool isDataFile(const MidiMessage &message, int dataTypeID) const override;
		virtual std::vector<std::shared_ptr<DataFile>> loadData(std::vector<MidiMessage> messages, int dataTypeID) const override;
		virtual std::vector<DataFileDescription> dataTypeNames() const override;

		// DataFileSendCapability
		virtual std::vector<MidiMessage> dataFileToMessages(std::shared_ptr<DataFile> dataFile, std::shared_ptr<SendTarget> target) const override;

		// DetailedParametersCapability
		std::vector<std::shared_ptr<SynthParameterDefinition>> allParameterDefinitions() const override;

		// BidirectionalSyncCapability
		virtual bool determineParameterChangeFromSysex(std::vector<juce::MidiMessage> const& messages, std::shared_ptr<SynthParameterDefinition> *outParam, int &outValue) override;

		// SendsProgramChangeCapability
		void gotProgramChange(MidiProgramNumber newNumber) override;
		MidiProgramNumber lastProgramChange() const override;

		// CreateInitPatchDataCapability
		virtual Synth::PatchData createInitPatch() override;

	private:
		void sendPatchToSynth(MidiController* controller, SimpleLogger* logger, MidiBuffer const& messages);

		friend class KawaiK3Control;
		friend class KawaiK3Parameter;

		// See Manual p. 49 for the sysex "commands"
		enum SysexFunction {
			ONE_BLOCK_DATA_REQUEST = 0,
			ALL_BLOCK_DATA_REQUEST = 1,
			PARAMETER_SEND = 16,
			ONE_BLOCK_DATA_DUMP = 32,
			ALL_BLOCK_DATA_DUMP = 33,
			WRITE_COMPLETE = 64,
			WRITE_ERROR = 65,
			WRITE_ERROR_BY_PROTECT = 66,
			WRITE_ERROR_BY_NO_CARTRIDGE = 67,
			MACHINE_ID_REQUEST = 96,
			MACHINE_ID_ACKNOWLEDE = 97,
			INVALID_FUNCTION = 255
		};
		static const std::set<SysexFunction> validSysexFunctions;

		std::vector<uint8> buildSysexFunction(SysexFunction function, uint8 subcommand) const;
		juce::MidiMessage buildSysexFunctionMessage(SysexFunction function, uint8 subcommand) const;
		SysexFunction sysexFunction(juce::MidiMessage const &message) const;
		uint8 sysexSubcommand(juce::MidiMessage const &message) const;

		MidiController::HandlerHandle k3BCRSyncHandler_ = MidiController::makeNoneHandle();

		MidiProgramNumber programNo_;
	};

}
