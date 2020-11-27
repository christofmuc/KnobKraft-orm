#include "Keystep.h"

#include "MidiHelpers.h"
#include "MidiController.h"
#include "Synth.h"

namespace midikraft {

	Keystep::Keystep() : midiController_(nullptr), currentSynth_(nullptr)
	{
	}

	Keystep::~Keystep()
	{
		if (midiController_) {
			midiController_->removeMessageHandler(callback);
		}
	}

	void Keystep::setMidiController(MidiController *midiController)
	{
		jassert(midiController_ == nullptr);

		midiController_ = midiController;

		// We want to receive MIDI messages as we might want to forward them to the active synth
		callback = MidiController::makeNoneHandle();
		midiController->addMessageHandler(callback, [this](MidiInput *source, MidiMessage const &message) {
			if (source->getName().toStdString() == midiInput()) {
				// That's a message from the Keystep. If it is not sysex, forward to the active synth
				if (!message.isSysEx()) {
					//TODO
					auto location = dynamic_cast<SimpleDiscoverableDevice *>(currentSynth_);
					if (location) {
						midiController_->getMidiOutput(location->midiOutput())->sendMessageNow(message);
					}
					else {
						midiController_->getMidiOutput("MXPXT: Sync In - Out All")->sendMessageNow(message);
					}
				}
			}
		});
	}

	void Keystep::switchToOutputChannel(Synth *controlledSynth)
	{
		auto location = dynamic_cast<SimpleDiscoverableDevice *>(controlledSynth);
		if (location) {
			changeOutputChannel(midiController_, location->channel(), []() {});
			currentSynth_ = controlledSynth;
		}
		else {
			jassertfalse;
		}
	}

	void Keystep::changeOutputChannel(MidiController *controller, MidiChannel newChannel, std::function<void()> onFinished)
	{
		ignoreUnused(controller);
		KeystepParam param;
		param.block = 0x41;
		param.paramNo = 0x16;
		param.value = (uint8)(newChannel.toZeroBasedInt() & 0xff);
		auto message = createSetParam(param);
		midiController_->getMidiOutput(midiOutput())->sendMessageNow(message);
		setCurrentChannelZeroBased(midiInput(), midiOutput(), newChannel.toZeroBasedInt());
	}

	MidiChannel Keystep::getOutputChannel() const
	{
		return channel();
	}

	void Keystep::setLocalControl(MidiController *controller, bool localControlOn)
	{
		ignoreUnused(controller, localControlOn);
		//TODO, this is in the wrong capability?
		// Noop, as the keystep has no sound generation
	}

	bool Keystep::getLocalControl() const
	{
		//TODO - that doesn't make sense for the keystep
		return false;
	}

	bool Keystep::hasLocalControl() const
	{
		return false;
	}

	std::string Keystep::getName() const
	{
		return "Arturia Keystep";
	}

	std::vector<juce::MidiMessage> Keystep::deviceDetect(int channel)
	{
		ignoreUnused(channel);
		// This works, but doesn't give me the MIDI channel I want.
		//return MidiHelpers::sysexMessage({ 0x7e, 0x7f, 0x06, 0x01 });
		// Ask explicitly for the MIDI channel parameter instead
		return { createRequestParam(0x41 /* whatever that is */, 0x16 /* That's called "user channel" */) };
	}

	int Keystep::deviceDetectSleepMS()
	{
		return 40;
	}

	MidiChannel Keystep::channelIfValidDeviceResponse(const MidiMessage &message)
	{
		KeystepParam param;
		if (getParam(message, param)) {
			// This is the reply to our request to send us the user MIDI channel (0x16)
			if (param.block == 0x41 && param.paramNo == 0x16) {
				return MidiChannel::fromZeroBase(param.value);
			}
		}
		return MidiChannel::invalidChannel();
	}

	bool Keystep::needsChannelSpecificDetection()
	{
		return false;
	}

	MidiMessage Keystep::createRequestParam(uint8 block, uint8 paramNo) const {
		std::vector<uint8> request({ 0x01 /* Request param */, 0x00, block, paramNo });
		return createSysexMessage(request);
	}

	MidiMessage Keystep::createSetParam(Keystep::KeystepParam const &param) const {
		std::vector<uint8> request({ 0x02 /* Set param */, 0x00, param.block, param.paramNo, param.value });
		return createSysexMessage(request);
	}

	MidiMessage Keystep::createSysexMessage(std::vector<uint8> const &message) const {
		std::vector<uint8> messageFrame({ 0x00, 0x20, 0x6b /* Arturia */, 0x7f, 0x42 /* Keystep 32? */ });
		std::copy(message.begin(), message.end(), std::back_inserter(messageFrame));
		return MidiHelpers::sysexMessage(messageFrame);
	}

	bool Keystep::isOwnSysex(MidiMessage const &message) const {
		auto expected = MidiHelpers::sysexMessage({ 0x00, 0x20, 0x6b /* Arturia */, 0x7f, 0x42 /* Keystep */ });
		return MidiHelpers::equalSysexMessageContent(message, expected, 5);
	}

	bool Keystep::getParam(MidiMessage const &message, Keystep::KeystepParam &outParam) const {
		if (getCommandCode(message) == 0x02) {
			if (message.getSysExDataSize() > 9) {
				outParam.block = message.getSysExData()[7];
				outParam.paramNo = message.getSysExData()[8];
				outParam.value = message.getSysExData()[9];
				return true;
			}
		}
		return false;
	}

	int Keystep::getCommandCode(MidiMessage const &message) const {
		if (isOwnSysex(message)) {
			if (message.getSysExDataSize() > 5) return message.getSysExData()[5];
		}
		return -1;
	}

}
