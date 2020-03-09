/*
   Copyright (c) 2019 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "JuceHeader.h"

#include "DSI.h"
#include "LayerCapability.h"
#include "DataFileLoadCapability.h"
#include "TypedNamedValue.h"

namespace midikraft {

	class Rev2 : public DSISynth, public LayerCapability, public DataFileLoadCapability, public DataFileSendCapability,
		private Value::Listener
	{
	public:
		// Data Item Types
		enum DataType {
			PATCH = 0, 
			GLOBAL_SETTINGS = 1,
			ALTERNATE_TUNING = 2
		};

		Rev2();

		// Basic Synth
		virtual std::string getName() const override;
		virtual std::shared_ptr<DataFile> patchFromPatchData(const Synth::PatchData &data, std::string const &name, MidiProgramNumber place) const override;
		virtual int numberOfBanks() const override;
		virtual int numberOfPatches() const override;
		virtual std::string friendlyBankName(MidiBankNumber bankNo) const override;

		// Edit Buffer Capability
		virtual std::shared_ptr<Patch> patchFromSysex(const MidiMessage& message) const override;
		virtual std::vector<MidiMessage> patchToSysex(const Patch &patch) const override;

		// Program Dump Capability
		virtual std::shared_ptr<Patch> patchFromProgramDumpSysex(const MidiMessage& message) const override;
		virtual std::vector<MidiMessage> patchToProgramDumpSysex(const Patch &patch) const override;

		MidiMessage patchPolySequenceToGatedTrack(const MidiMessage& message, int gatedSeqTrack);
		MidiMessage clearPolySequencer(const MidiMessage &programEditBuffer, bool layerA, bool layerB);
		MidiMessage copySequencersFromOther(const MidiMessage& currentProgram, const MidiMessage &lockedProgram);

		// LayerCapability
		virtual void switchToLayer(int layerNo) override;

		// SoundExpanderCapability
		virtual void changeInputChannel(MidiController *controller, MidiChannel channel, std::function<void()> onFinished) override;
		virtual void setMidiControl(MidiController *controller, bool isOn) override;

		// MasterkeyboardCapability
		virtual void changeOutputChannel(MidiController *controller, MidiChannel channel, std::function<void()> onFinished) override;
		virtual void setLocalControl(MidiController *controller, bool localControlOn) override;

		virtual PatchData filterVoiceRelevantData(std::shared_ptr<DataFile> unfilteredData) const override;

		// DataFileLoadCapability - this is used for loading the GlobalSettings from the synth for the property editor
		std::vector<MidiMessage> requestDataItem(int itemNo, int dataTypeID) override;
		int numberOfDataItemsPerType(int dataTypeID) const override;
		bool isDataFile(const MidiMessage &message, int dataTypeID) const override;
		std::vector<std::shared_ptr<DataFile>> loadData(std::vector<MidiMessage> messages, int dataTypeID) const override;
		std::vector<DataFileDescription> dataTypeNames() const override;

		// DataFileSendCapability
		std::vector<MidiMessage> dataFileToMessages(std::shared_ptr<DataFile> dataFile) const override;

		// Access to global settings for the property editor
		void setGlobalSettingsFromDataFile(std::shared_ptr<DataFile> dataFile);
		std::vector<std::shared_ptr<TypedNamedValue>> getGlobalSettings();

	private:
		MidiMessage buildSysexFromEditBuffer(std::vector<uint8> editBuffer);
		MidiMessage filterProgramEditBuffer(const MidiMessage &programEditBuffer, std::function<void(std::vector<uint8> &)> filterExpressionInPlace);

		void initGlobalSettings();
		
		void valueChanged(Value& value) override; // Value::Listener that gets called when someone changes a global setting value

		// That's not very Rev2 specific
		static uint8 clamp(int value, uint8 min = 0, uint8 max = 127);

		std::vector<std::shared_ptr<TypedNamedValue>> globalSettings_;
	};

}
