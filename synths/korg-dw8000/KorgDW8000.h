/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "Synth.h"
#include "Patch.h"
#include "ReadonlySoundExpander.h"
//#include "SupportedByBCR2000.h"
#include "EditBufferCapability.h"

namespace midikraft {

	class KorgDW8000Patch;

	class KorgDW8000 : public Synth /*, public SupportedByBCR2000 */, public SimpleDiscoverableDevice, public EditBufferCapability, public ReadonlySoundExpander {
	public:

		// Basic Synth implementation
		virtual std::string getName() const override;
		virtual bool isOwnSysex(MidiMessage const &message) const override;
		virtual int numberOfBanks() const override;
		virtual int numberOfPatches() const override;
		virtual std::string friendlyProgramName(MidiProgramNumber programNo) const override;
		virtual std::string friendlyBankName(MidiBankNumber bankNo) const override;

		virtual std::shared_ptr<DataFile> patchFromPatchData(const Synth::PatchData &data, MidiProgramNumber place) const override;

		// Discoverable Device
		virtual std::vector<juce::MidiMessage> deviceDetect(int channel) override;
		virtual int deviceDetectSleepMS() override;
		virtual MidiChannel channelIfValidDeviceResponse(const MidiMessage &message) override;
		virtual bool needsChannelSpecificDetection() override;

		// Edit Buffer Capability
		virtual MidiMessage requestEditBufferDump() const override;
		virtual bool isEditBufferDump(const MidiMessage& message) const override;
		virtual std::shared_ptr<DataFile> patchFromSysex(const MidiMessage& message) const override;
		virtual std::vector<MidiMessage> patchToSysex(std::shared_ptr<DataFile> patch) const override;
		virtual MidiMessage saveEditBufferToProgram(int programNumber) override;

		// SoundExpanderCapability
		virtual MidiChannel getInputChannel() const override;

		// Implementation of BCR2000 sync
	/*	virtual std::vector<std::string> presetNames() override;
		virtual void setupBCR2000(MidiController *controller, BCR2000 &bcr, SimpleLogger *logger) override;
		virtual void syncDumpToBCR(MidiProgramNumber programNumber, MidiController *controller, BCR2000 &bcr, SimpleLogger *logger) override;
		virtual void setupBCR2000View(BCR2000_Component &view) override;
		virtual void setupBCR2000Values(BCR2000_Component &view, Patch *patch) override;*/

		static std::vector<float> romWave(int waveNo);
		static std::string waveName(int waveNo);
		void createReverseEngineeringData();

	private:
		enum SysexCommand {
			DATA_SAVE_REQUEST = 0x10,
			WRITE_REQUEST = 0x11,
			DATA_DUMP = 0x40
		};
	};

}
