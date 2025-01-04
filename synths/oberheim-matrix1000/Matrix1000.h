/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "JuceHeader.h"

#include "Synth.h"
#include "Patch.h"
#include "HasBanksCapability.h"
#include "EditBufferCapability.h"
#include "ProgramDumpCapability.h"
#include "StreamLoadCapability.h"
#include "SoundExpanderCapability.h"
#include "GlobalSettingsCapability.h"
//#include "SupportedByBCR2000.h"

#include "MidiController.h"

namespace midikraft {

	class Matrix1000_GlobalSettings_Loader;

	class Matrix1000 : public Synth, /* public SupportedByBCR2000, */
		public SimpleDiscoverableDevice,
		public HasBanksCapability, 
		public EditBufferCapability, public ProgramDumpCabability, public StreamLoadCapability, // pretty complete MIDI implementation of the Matrix1000
		public SoundExpanderCapability, public GlobalSettingsCapability
	{
	public:
		Matrix1000();
		virtual ~Matrix1000() override;

		// Basic Synth implementation
		virtual std::string getName() const override;
		virtual bool isOwnSysex(MidiMessage const &message) const override;
		virtual std::string friendlyProgramName(MidiProgramNumber programNo) const override;
		virtual std::shared_ptr<DataFile> patchFromPatchData(const Synth::PatchData& data, MidiProgramNumber place) const override;
		virtual PatchData filterVoiceRelevantData(std::shared_ptr<DataFile> unfilteredData) const override;

		// HasBanksCapability
		virtual int numberOfBanks() const override;
		virtual int numberOfPatches() const override;
		virtual std::string friendlyBankName(MidiBankNumber bankNo) const override;
		virtual std::vector<juce::MidiMessage> bankSelectMessages(MidiBankNumber bankNo) const override;
		
		// Edit Buffer Capability
		virtual std::vector<MidiMessage> requestEditBufferDump() const override;
		virtual bool isEditBufferDump(const std::vector<MidiMessage>& message) const override;
		virtual std::shared_ptr<DataFile> patchFromSysex(const std::vector<MidiMessage>& message) const override;
		virtual std::vector<MidiMessage> patchToSysex(std::shared_ptr<DataFile> patch) const override;
		virtual MidiMessage saveEditBufferToProgram(int programNumber) override;

		// Program Dump Capability
		virtual std::vector<MidiMessage> requestPatch(int patchNo) const override;
		virtual bool isSingleProgramDump(const std::vector<MidiMessage>& message) const override;
		virtual MidiProgramNumber getProgramNumber(const std::vector<MidiMessage>&message) const override;
		virtual std::shared_ptr<DataFile> patchFromProgramDumpSysex(const std::vector<MidiMessage>& message) const override;
		virtual std::vector<MidiMessage> patchToProgramDumpSysex(std::shared_ptr<DataFile> patch, MidiProgramNumber programNumber) const override;

		// StreamLoadCapability
		virtual std::vector<MidiMessage> requestStreamElement(int number, StreamType streamType) const override;
		virtual int numberOfStreamMessagesExpected(StreamType streamType) const override;
		virtual bool isMessagePartOfStream(const MidiMessage& message, StreamType streamType) const override;
		virtual bool isStreamComplete(std::vector<MidiMessage> const &messages, StreamType streamType) const override;
		virtual bool shouldStreamAdvance(std::vector<MidiMessage> const &messages, StreamType streamType) const override;
		virtual TPatchVector loadPatchesFromStream(std::vector<MidiMessage> const &streamDump) const override;

		// SoundExpanderCapability
		virtual bool canChangeInputChannel() const override;
		virtual void changeInputChannel(MidiController *controller, MidiChannel channel, std::function<void()> onFinished) override;
		virtual MidiChannel getInputChannel() const override;
		virtual bool hasMidiControl() const override;
		virtual bool isMidiControlOn() const override;
		virtual void setMidiControl(MidiController *controller, bool isOn) override;

		// Sync with BCR2000
/*		virtual std::vector<std::string> presetNames() override;
		virtual void setupBCR2000(MidiController *controller, BCR2000 &bcr, SimpleLogger *logger) override;
		virtual void syncDumpToBCR(MidiProgramNumber programNumber, MidiController *controller, BCR2000 &bcr, SimpleLogger *logger) override;
		virtual void setupBCR2000View(BCR2000_Component &view) override;
		virtual void setupBCR2000Values(BCR2000_Component &view, Patch *patch) override;*/

		// Discoverable Device
		virtual std::vector<juce::MidiMessage> deviceDetect(int channel) override;
		virtual int deviceDetectSleepMS() override;
		virtual MidiChannel channelIfValidDeviceResponse(const MidiMessage &message) override;
		virtual bool needsChannelSpecificDetection() override;

		// GlobalSettingsCapability
		virtual void setGlobalSettingsFromDataFile(std::shared_ptr<DataFile> dataFile) override;
		virtual std::vector<std::shared_ptr<TypedNamedValue>> getGlobalSettings() override;
		virtual DataFileLoadCapability *loader() override;
		virtual int settingsDataFileType() const override;

		// Matrix1000 specific functions
		bool isSplitPatch(MidiMessage const &message) const;

		//private: Only for testing public
		PatchData unescapeSysex(const uint8 *sysExData, int sysExLen) const;
		std::vector<uint8> escapeSysex(const PatchData &programEditBuffer) const;

	private:
		friend class Matrix1000_GlobalSettings_Loader;

		enum Matrix1000_DataFileType {
			PATCH = 0,
			DF_MATRIX1000_SETTINGS = 1
		};
		enum REQUEST_TYPE {
			BANK_AND_MASTER = 0x00,
			SINGLE_PATCH = 0x01,
			MASTER = 0x03,
			EDIT_BUFFER = 0x04
		};

		MidiMessage createRequest(REQUEST_TYPE typeNo, uint8 number) const;
		MidiMessage createBankSelect(MidiBankNumber bankNo) const;
		MidiMessage createBankUnlock() const;

		MidiController::HandlerHandle matrixBCRSyncHandler_ = MidiController::makeNoneHandle();

		// This listener implements sending update messages via NRPN when any of the global settings is changed via the UI
		class GlobalSettingsListener : public ValueTree::Listener {
		public:
			GlobalSettingsListener(Matrix1000 *synth) : synth_(synth) {}
			void valueTreePropertyChanged(ValueTree& treeWhosePropertyHasChanged, const Identifier& property) override;

			std::vector<uint8> globalSettingsData_; // The global settings data as last retrieved from the synth

		private:
			Matrix1000 *synth_;
		};

		void initGlobalSettings();

		Matrix1000_GlobalSettings_Loader *globalSettingsLoader_; // Sort of a pimpl pattern
		TypedNamedValueSet globalSettings_;
		ValueTree globalSettingsTree_;
		GlobalSettingsListener updateSynthWithGlobalSettingsListener_;
		
	};

}
