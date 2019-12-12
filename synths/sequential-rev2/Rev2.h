/*
   Copyright (c) 2019 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "JuceHeader.h"

#include "DSI.h"
#include "LayerCapability.h"

namespace midikraft {

	class Rev2 : public DSISynth, public LayerCapability {
	public:
		Rev2();

		// Basic Synth
		virtual std::string getName() const override;
		virtual std::shared_ptr<Patch> patchFromPatchData(const Synth::PatchData &data, std::string const &name, MidiProgramNumber place) const override;
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

		virtual PatchData filterVoiceRelevantData(PatchData const &unfilteredData) const override;

	private:
		MidiMessage buildSysexFromEditBuffer(std::vector<uint8> editBuffer);
		juce::MidiMessage filterProgramEditBuffer(const MidiMessage &programEditBuffer, std::function<void(std::vector<uint8> &)> filterExpressionInPlace);

		// That's not very Rev2 specific
		static uint8 clamp(int value, uint8 min = 0, uint8 max = 127);

		// Debug tools
		static void compareMessages(const MidiMessage &msg1, const MidiMessage &msg2);
		std::string versionString_;
	};

}
