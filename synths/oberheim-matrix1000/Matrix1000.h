#pragma once

#include "Synth.h"
#include "Patch.h"
#include "SynthPatchCapability.h"
#include "EditBufferCapability.h"
#include "ProgramDumpCapability.h"
#include "BankDumpCapability.h"
#include "SoundExpanderCapability.h"
#include "SupportedByBCR2000.h"

#include "EditBufferHandler.h"

class Matrix1000 : public Synth, public SupportedByBCR2000, public SynthPatchCapability, 
	public EditBufferCapability, public ProgramDumpCabability, public BankDumpCapability, // pretty complete MIDI implementation of the Matrix1000
	public SoundExpanderCapability 
{
public:

	// Basic Synth implementation
	virtual std::string getName() const override;
	virtual bool isOwnSysex(MidiMessage const &message) const override;
	virtual int numberOfBanks() const override;
	virtual int numberOfPatches() const override;
	virtual std::string friendlyBankName(MidiBankNumber bankNo) const override;
	virtual std::shared_ptr<Patch> patchFromPatchData(const Synth::PatchData &data, std::string const &name, MidiProgramNumber place) const override;
	virtual PatchData filterVoiceRelevantData(PatchData const &unfilteredData) const override;

	// Edit Buffer Capability
	virtual MidiMessage requestEditBufferDump() override;
	virtual bool isEditBufferDump(const MidiMessage& message) const override;
	virtual std::shared_ptr<Patch> patchFromSysex(const MidiMessage& message) const override;
	virtual std::vector<MidiMessage> patchToSysex(const Patch &patch) const override;
	virtual MidiMessage saveEditBufferToProgram(int programNumber) override;

	// Program Dump Capability
	virtual std::vector<MidiMessage> requestPatch(int patchNo) override;
	virtual bool isSingleProgramDump(const MidiMessage& message) const override;
	virtual std::shared_ptr<Patch> patchFromProgramDumpSysex(const MidiMessage& message) const;
	virtual std::vector<MidiMessage> patchToProgramDumpSysex(const Patch &patch) const;

	// Bank Dump Capability
	virtual MidiMessage requestBankDump(MidiBankNumber bankNo) const override;
	virtual bool isBankDump(const MidiMessage& message) const override;
	virtual bool isBankDumpFinished(std::vector<MidiMessage> const &bankDump) const;
	virtual TPatchVector patchesFromSysexBank(const MidiMessage& message) const override;

	std::string patchToText(PatchData const &patch);

	//! Not implemented yet
	virtual std::shared_ptr<Patch> synthSetupToPatch(SynthSetup const &sound, std::function<void(std::string warning)> logWarning) override;
	
	// SoundExpanderCapability
	virtual bool canChangeInputChannel() const override;
	virtual void changeInputChannel(MidiController *controller, MidiChannel channel) override;
	virtual MidiChannel getInputChannel() const override;
	virtual bool hasMidiControl() const override;
	virtual bool isMidiControlOn() const override;
	virtual void setMidiControl(MidiController *controller, bool isOn) override;

	// Sync with BCR2000
	virtual std::vector<std::string> presetNames() override;
	virtual void setupBCR2000(MidiController *controller, BCR2000 &bcr, SimpleLogger *logger) override;
	virtual void syncDumpToBCR(MidiProgramNumber programNumber, MidiController *controller, BCR2000 &bcr, SimpleLogger *logger) override;
	virtual void setupBCR2000View(BCR2000_Component &view) override;
	virtual void setupBCR2000Values(BCR2000_Component &view, Patch *patch) override;

	// Discoverable Device
	virtual MidiMessage deviceDetect(int channel) override;
	virtual int deviceDetectSleepMS() override;
	virtual MidiChannel channelIfValidDeviceResponse(const MidiMessage &message) override;
	virtual bool needsChannelSpecificDetection() override;

//private: Only for testing public
	PatchData unescapeSysex(const uint8 *sysExData, int sysExLen) const;
	std::vector<uint8> escapeSysex(const PatchData &programEditBuffer) const;


	virtual SynthSetup patchToSynthSetup(Synth::PatchData const &patch) override;

private:
	enum REQUEST_TYPE {
		BANK_AND_MASTER = 0x00,
		SINGLE_PATCH = 0x01,
		MASTER = 0x03,
		EDIT_BUFFER = 0x04
	};

	MidiMessage createRequest(REQUEST_TYPE typeNo, uint8 number) const;
	MidiMessage createBankSelect(MidiBankNumber bankNo) const;
	MidiMessage createBankUnlock() const;

	EditBufferHandler::HandlerHandle matrixBCRSyncHandler_ = EditBufferHandler::makeNone();
};


