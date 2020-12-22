/*
   Copyright (c) 2019 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "Synth.h"

#include "EditBufferCapability.h"
#include "ProgramDumpCapability.h"
#include "SoundExpanderCapability.h"
#include "MasterkeyboardCapability.h"
#include "GlobalSettingsCapability.h"

#include "TypedNamedValue.h"

namespace midikraft {

	// Global constants
	extern std::map<int, std::string> kDSIAlternateTunings();

	struct DSIGlobalSettingDefinition {
		int sysexIndex;
		int nrpn;
		TypedNamedValue typedNamedValue;
		int displayOffset = 0;
	};

	class DSISynth : public Synth, public SimpleDiscoverableDevice, public EditBufferCapability, public ProgramDumpCabability,
		public SoundExpanderCapability, public MasterkeyboardCapability, public KeyboardCapability, public GlobalSettingsCapability {
	public:
		// Basic Synth
		virtual bool isOwnSysex(MidiMessage const &message) const override;

		// Discoverable Device
		virtual std::vector<juce::MidiMessage> deviceDetect(int channel) override;
		virtual int deviceDetectSleepMS() override;
		virtual MidiChannel channelIfValidDeviceResponse(const MidiMessage &message) override;
		virtual bool needsChannelSpecificDetection() override;

		// Edit Buffer Capability
		virtual MidiMessage requestEditBufferDump() override;
		virtual bool isEditBufferDump(const MidiMessage& message) const override;
		virtual MidiMessage saveEditBufferToProgram(int programNumber) override;

		// Program Dump Capability
		virtual std::vector<MidiMessage> requestPatch(int patchNo) const override;
		virtual bool isSingleProgramDump(const MidiMessage& message) const override;
		virtual MidiProgramNumber getProgramNumber(const MidiMessage &message) const override;

		// MasterkeyboadCapability, common
		virtual MidiChannel getOutputChannel() const override;
		virtual bool hasLocalControl() const override;
		virtual bool getLocalControl() const override;

		// KeyboardCapability
		virtual bool hasKeyboard() const override;

		// SoundExpanderCapability, common
		virtual bool canChangeInputChannel() const override;
		virtual MidiChannel getInputChannel() const override;
		virtual bool hasMidiControl() const override;
		virtual bool isMidiControlOn() const override;

		// GlobalSettingsCapability
		virtual void setGlobalSettingsFromDataFile(std::shared_ptr<DataFile> dataFile) override;
		virtual std::vector<std::shared_ptr<TypedNamedValue>> getGlobalSettings() override;

		// Implement this to get the common global settings implementation working
		virtual std::vector<DSIGlobalSettingDefinition> dsiGlobalSettings() const = 0;

	protected:
		DSISynth(uint8 midiModelID);

		MidiBuffer createNRPN(int parameterNo, int value);
		static PatchData unescapeSysex(const uint8 *sysExData, int sysExLen, int expectedLength);
		static std::vector<uint8> escapeSysex(const PatchData &programEditBuffer, size_t bytesToEscape);

		uint8 midiModelID_;
		std::string versionString_;
		bool localControl_;
		bool midiControl_;

		// This listener implements sending update messages via NRPN when any of the global settings is changed via the UI
		class GlobalSettingsListener : public ValueTree::Listener {
		public:
			GlobalSettingsListener(DSISynth *synth) : synth_(synth) {}
			void valueTreePropertyChanged(ValueTree& treeWhosePropertyHasChanged, const Identifier& property) override;
		private:
			DSISynth *synth_;
		};
		
		TypedNamedValueSet globalSettings_;
		ValueTree globalSettingsTree_;
		GlobalSettingsListener updateSynthWithGlobalSettingsListener_;
	};

}
