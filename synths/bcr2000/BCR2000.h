/*
   Copyright (c) 2019 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "JuceHeader.h"

#include "SimpleDiscoverableDevice.h"
#include "MidiController.h"
#include "MidiChannel.h"
#include "HasBanksCapability.h"
#include "StreamLoadCapability.h"
#include "DataFileLoadCapability.h"
#include "DataFileSendCapability.h"
#include "StoredPatchNameCapability.h"

#include "Logger.h"

#include <vector>
#include <string>

namespace midikraft {

	class BCRdefinition;

	class BCR2000 : public Synth, public HasBanksCapability, public SimpleDiscoverableDevice, public StreamLoadCapability, public DataFileSendCapability {
	public:
		struct BCRError {
			uint8 errorCode;
			std::string errorText;
			int lineNumber;
			std::string lineText;

			std::string toDisplayString() const;
		};

		void writeToFile(std::string const &filename, std::string const &bcl) const;
		std::vector<MidiMessage> convertToSyx(std::string const &bcl, bool verbatim = false) const;

		static std::string convertSyxToText(const MidiMessage &message);
		std::string findPresetName(std::vector<MidiMessage> const &messages) const;
		static bool isSysexFromBCR2000(const MidiMessage& message);

		void sendSysExToBCR(std::shared_ptr<SafeMidiOutput> midiOutput, std::vector<MidiMessage> const &messages, std::function<void(std::vector<BCRError> const &errors)> const whenDone);

		// Implementation of DiscoverableDevice
		virtual std::string getName() const override;
		virtual std::vector<juce::MidiMessage> deviceDetect(int channel) override;
		virtual int deviceDetectSleepMS() override;
		virtual MidiChannel channelIfValidDeviceResponse(const MidiMessage &message) override;
		virtual bool needsChannelSpecificDetection() override;

		static std::string generateBCRHeader();
		static std::string generateBCREnd(int recallPreset);
		static std::string generatePresetHeader(std::string const &presetName);
		static std::string generateBCRFooter(int storagePlace);
		static std::string generateAllEncoders(std::vector<std::pair<BCRdefinition *, std::string>> &all_entries);

		static std::string syxToBCRString(MidiMessage const &syx);

		// Preset handling in the BCR2000 itself. This is fully automated management, so we do not need a Librarian
		std::vector<std::string> listOfPresets() const;
		int indexOfPreset(std::string const &name) const;
		void selectPreset(MidiController *controller, int presetIndex);
		void refreshListOfPresets(std::function<void()> callback);
		void invalidateListOfPresets();

		// Unused so far
		MidiMessage requestEditBuffer() const;

		// StreamLoadCapability
		virtual std::vector<MidiMessage> requestStreamElement(int elemNo, StreamType streamType) const override;
		virtual int numberOfStreamMessagesExpected(StreamType streamType) const override;
		virtual bool isMessagePartOfStream(MidiMessage const &message, StreamType streamType) const override;
		virtual bool isStreamComplete(std::vector<MidiMessage> const &messages, StreamType streamType) const override;
		virtual bool shouldStreamAdvance(std::vector<MidiMessage> const &messages, StreamType streamType) const override;
		virtual TPatchVector loadPatchesFromStream(std::vector<MidiMessage> const &streamDump) const override;

		// DataFileSendCapability
		virtual std::vector<MidiMessage> dataFileToMessages(std::shared_ptr<DataFile> dataFile, std::shared_ptr<SendTarget> target) const override;

		// Synth
		int numberOfBanks() const override;
		int numberOfPatches() const override;
		std::string friendlyBankName(MidiBankNumber bankNo) const override;
		virtual std::vector<juce::MidiMessage> bankSelectMessages(MidiBankNumber bankNo) const;
		std::shared_ptr<DataFile> patchFromPatchData(const Synth::PatchData &data, MidiProgramNumber place) const override;
		bool isOwnSysex(MidiMessage const &message) const override;

		// Override generic message generation hander because we have our own way to store the data
		virtual std::vector<MidiMessage> dataFileToSysex(std::shared_ptr<DataFile> dataFile, std::shared_ptr<SendTarget> target) override;
		// Override the generic send handler so we can do the proper error handling for the BCR2000
		virtual void sendDataFileToSynth(std::shared_ptr<DataFile> dataFile, std::shared_ptr<SendTarget> target) override;

	private:
		uint8 sysexCommand(const MidiMessage &message) const;
		std::vector<uint8> createSysexCommandData(uint8 commandCode) const;
		std::vector<std::string> bcrPresets_; // These are the names of the 32 presets stored in the BCR2000
		std::vector<BCRError> errorsDuringUpload_; // Make sure to not run two uploads in parallel...

		struct TransferCounters {
			int numMessages;
			int receivedMessages;
			int lastLine;
			int overflowCounter;
		};
	};

	class BCR2000Preset : public DataFile, public StoredPatchNameCapability {
	public:
		BCR2000Preset(std::string const &name, Synth::PatchData const &data);

		virtual std::string name() const override;
		virtual void setName(std::string const& name) override;

	private:
		std::string name_;
	};
}
