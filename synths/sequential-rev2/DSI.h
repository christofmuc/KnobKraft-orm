#pragma once

#include "Synth.h"

#include "EditBufferCapability.h"
#include "ProgramDumpCapability.h"
#include "SoundExpanderCapability.h"
#include "MasterkeyboardCapability.h"

namespace midikraft {

	class DSISynth : public Synth, public EditBufferCapability, public ProgramDumpCabability, public SoundExpanderCapability, public MasterkeyboardCapability {
	public:
		DSISynth(uint8 midiModelID);

		// Basic Synth
		virtual bool isOwnSysex(MidiMessage const &message) const override;

		// Discoverable Device
		virtual MidiMessage deviceDetect(int channel) override;
		virtual int deviceDetectSleepMS() override;
		virtual MidiChannel channelIfValidDeviceResponse(const MidiMessage &message) override;
		virtual bool needsChannelSpecificDetection() override;

		// Edit Buffer Capability
		virtual MidiMessage requestEditBufferDump() override;
		virtual bool isEditBufferDump(const MidiMessage& message) const override;
		virtual MidiMessage saveEditBufferToProgram(int programNumber) override;

		// Program Dump Capability
		virtual std::vector<MidiMessage> requestPatch(int patchNo) override;
		virtual bool isSingleProgramDump(const MidiMessage& message) const override;

		// MasterkeyboadCapability, common
		virtual MidiChannel getOutputChannel() const override;
		virtual bool hasLocalControl() const override;
		virtual bool getLocalControl() const override;

		// SoundExpanderCapability, common
		virtual bool canChangeInputChannel() const override;
		virtual MidiChannel getInputChannel() const override;
		virtual bool hasMidiControl() const override;
		virtual bool isMidiControlOn() const override;

	protected:
		MidiBuffer createNRPN(int parameterNo, int value);
		static PatchData unescapeSysex(const uint8 *sysExData, int sysExLen, int expectedLength);
		static std::vector<uint8> escapeSysex(const PatchData &programEditBuffer);

		uint8 midiModelID_;
		std::string versionString_;
		bool localControl_;
		bool midiControl_;
	};

}
