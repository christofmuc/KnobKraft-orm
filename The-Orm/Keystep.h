#pragma once

#include "JuceHeader.h"

#include "SimpleDiscoverableDevice.h"
#include "MidiController.h"
#include "MasterkeyboardCapability.h"

namespace midikraft {

	class Synth;

	class Keystep : public SimpleDiscoverableDevice, public MasterkeyboardCapability {
	public:
		Keystep();
		virtual ~Keystep();
		void setMidiController(MidiController *midiController);

		// Switch channel to Synth
		void switchToOutputChannel(Synth *controlledSynth);

		// Master keyboard Capability
		virtual void changeOutputChannel(MidiController *controller, MidiChannel newChannel, std::function<void()> onFinished) override;
		virtual MidiChannel getOutputChannel() const;
		virtual void setLocalControl(MidiController *controller, bool localControlOn) override;
		virtual bool getLocalControl() const override;
		virtual bool hasLocalControl() const override;

		// DiscoverableDevice
		virtual std::string getName() const override;
		virtual std::vector<MidiMessage> deviceDetect(int channel) override;
		virtual int deviceDetectSleepMS() override;
		virtual MidiChannel channelIfValidDeviceResponse(const MidiMessage &message) override;
		virtual bool needsChannelSpecificDetection() override;

	private:
		struct KeystepParam {
			uint8 block;
			uint8 paramNo;
			uint8 value;
		};
		MidiMessage createRequestParam(uint8 block, uint8 paramNo) const;
		MidiMessage createSetParam(Keystep::KeystepParam const &param) const;
		MidiMessage createSysexMessage(std::vector<uint8> const &message) const;
		bool isOwnSysex(MidiMessage const &message) const;
		bool getParam(MidiMessage const &message, Keystep::KeystepParam &outParam) const;
		int getCommandCode(MidiMessage const &message) const;

		MidiController *midiController_;
		MidiController::HandlerHandle callback = MidiController::makeNoneHandle();
		Synth *currentSynth_;
	};

}
