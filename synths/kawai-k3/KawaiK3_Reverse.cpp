#include "KawaiK3_Reverse.h"

#include "KawaiK3Patch.h"
#include "Sysex.h"
#include "Logger.h"

KawaiK3_Reverse::KawaiK3_Reverse(KawaiK3 &k3) : k3_(k3)
{
}

// As I figured out that the sysex documentation in the Kawai K3 manual was wrong, I had to investigate with the help of this 
// extra code what the real deal was. The result is now encoded in the KawaiK3Parameter::allParameters setup

juce::MidiMessage KawaiK3_Reverse::emptyTone() {
	// Create completely empty patch - 34 bytes 0 
	std::vector<uint8> data(34);
	return k3_.patchToProgramDumpSysex(KawaiK3Patch(MidiProgramNumber::fromZeroBase(0), data))[0];
}

void KawaiK3_Reverse::createReverseEngineeringData(MidiOutput *midiOutput, EditBufferHandler &continuationHandler, SimpleLogger &logger) {
	// Determine what we will do with the answer...
	handle_ = EditBufferHandler::makeOne();
	handler_ = &continuationHandler;

	continuationHandler.setNextEditBufferHandler(handle_, [this, midiOutput, &logger](const juce::MidiMessage &editBuffer) {
		this->handleNextEditBufferDump(midiOutput, editBuffer, logger);
	});

	// Clear patch
	midiOutput->sendMessageNow(emptyTone()); // This modifies program slot #0
											
	// Now set each parameter to its number using the send sysex commands
	int value = 1;
	for (auto param : KawaiK3Parameter::allParameters) {
		int setValue = std::min(value, param->maxValue());
		logger.postMessage((boost::format("Sending MIDI message to set parameter %s to %d (%d)") % param->name() % setValue % value).str());
		MidiMessage message;
		if (param->minValue() < 0) {
			message = k3_.createSetParameterMessage(param, setValue - param->minValue());
		}
		else {
			message = k3_.createSetParameterMessage(param, setValue);
		}
		logger.postMessage(message.getDescription());
		midiOutput->sendMessageNow(message); // Modify edit buffer
		value++;
		if (value > 31) value = 1;
	}
	logger.postMessage("Please be quick and save to program #1"); // Yes, you have to manually do it, as there is no MIDI command to store the edit buffer (no write request)
	Thread::sleep(5000);

	// Now query the sysex data back and hope the set value is in there
	midiOutput->sendMessageNow(k3_.requestBankDump(MidiBankNumber::fromZeroBase(0)));
}

void KawaiK3_Reverse::handleNextEditBufferDump(MidiOutput *, juce::MidiMessage editBuffer, SimpleLogger &) {
	// Now inspect the sysex dump
	if (k3_.isBankDump(editBuffer)) {
		auto patch = k3_.patchFromProgramDumpSysex(editBuffer);
		int value = 1;
		for (auto param : KawaiK3Parameter::allParameters) {
			if (param->paramNo() >= 0 && param->paramNo() != KawaiK3Parameter::MONO) { // The MONO parameter can not be set by the sysex change parameter...
				int soll = std::max(param->minValue(), std::min(value, param->maxValue()));
				auto ist = patch->value(*param);
				//auto raw = patch->data()[param->sysexIndex() - 1];
				jassert(ist == soll);
				ist = patch->value(*param);
			}
			value++;
			if (value > 31) value = 1;
		}
		// Done
		handler_->removeEditBufferHandler(handle_);
	}
}
