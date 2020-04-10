#pragma once

#include "JuceHeader.h"

#include "Synth.h"
#include "EditBufferCapability.h"
#include "HandshakeLoadingCapability.h"
#include "SoundExpanderCapability.h"
#include "SupportedByBCR2000.h"

#include "MKS80_Parameter.h"

class MKS80 : public Synth, public EditBufferCapability, public HandshakeLoadingCapability, public SoundExpanderCapability, public SupportedByBCR2000 {
public:
	MKS80();

	virtual int numberOfBanks() const override;
	virtual int numberOfPatches() const override;
	virtual std::string friendlyBankName(MidiBankNumber bankNo) const override;

	virtual std::shared_ptr<Patch> patchFromPatchData(const Synth::PatchData &data, std::string const &name, MidiProgramNumber place) const override;

	virtual bool isOwnSysex(MidiMessage const &message) const override;

	virtual MidiMessage deviceDetect(int channel) override;
	virtual int deviceDetectSleepMS() override;
	virtual MidiChannel channelIfValidDeviceResponse(const MidiMessage &message) override;
	virtual bool needsChannelSpecificDetection() override;
	virtual bool endDeviceDetect(MidiMessage &endMessageOut) const override;

	virtual std::string getName() const override;

	// Edit Buffer Capability
	virtual MidiMessage requestEditBufferDump() override;
	virtual bool isEditBufferDump(const MidiMessage& message) const override;
	virtual std::shared_ptr<Patch> patchFromSysex(const MidiMessage& message) const override;
	virtual std::vector<MidiMessage> patchToSysex(const Patch &patch) const override;
	virtual MidiMessage saveEditBufferToProgram(int programNumber) override;

	// HandshakeLoadingCapability
	virtual std::shared_ptr<ProtocolState> createStateObject() override;
	virtual void startDownload(SafeMidiOutput *output, std::shared_ptr<ProtocolState> saveState) override;
	virtual bool isNextMessage(MidiMessage const &message, std::vector<MidiMessage> &answer, std::shared_ptr<ProtocolState> state) override;

	// Sound expander capability
	virtual bool canChangeInputChannel() const override;
	virtual void changeInputChannel(MidiController *controller, MidiChannel channel) override;
	virtual MidiChannel getInputChannel() const override;
	virtual bool hasMidiControl() const override;
	virtual bool isMidiControlOn() const override;
	virtual void setMidiControl(MidiController *controller, bool isOn) override;


	// Override for funky formats
	TPatchVector loadSysex(std::vector<MidiMessage> const &sysexMessages) override;

	// SupportedByBCR2000
	std::vector<std::string> presetNames() override;

	void setupBCR2000(MidiController *controller, BCR2000 &bcr, SimpleLogger *logger) override;
	void syncDumpToBCR(MidiProgramNumber programNumber, MidiController *controller, BCR2000 &bcr, SimpleLogger *logger) override;
	void setupBCR2000View(BCR2000_Component &view) override;
	void setupBCR2000Values(BCR2000_Component &view, Patch *patch) override;

private:
	std::string presetName();

	static uint8 rolandChecksum(std::vector<uint8>::iterator start, std::vector<uint8>::iterator end);

	MidiMessage buildHandshakingMessage(MKS80_Operation_Code code) const;
	MidiMessage buildHandshakingMessage(MKS80_Operation_Code code, MidiChannel channel) const;

	MKS80_Operation_Code getSysexOperationCode(MidiMessage const &message) const;
};
