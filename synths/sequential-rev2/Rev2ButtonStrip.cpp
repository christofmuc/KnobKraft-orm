#include "Rev2ButtonStrip.h"

#include "EditBufferHandler.h"
#include "MidiController.h"
#include "Logger.h"
#include "Rev2.h"
#include "Rev2Patch.h"

Rev2ButtonStrip::Rev2ButtonStrip(Rev2 &rev2, MidiController *controller, EditBufferHandler *handler, SimpleLogger *logger) : 
	LambdaButtonStrip(), isLocked_(false), handler_(handler), currentPatch_(nullptr)
{
	LambdaButtonStrip::TButtonMap buttonDefs = {
	{ "poly2gate",{ "Copy Poly to Gated", [this, controller, handler, logger, &rev2]() {
		auto handle = EditBufferHandler::makeOne();
		handler->setNextEditBufferHandler(handle, [this, handle, controller, handler, logger, &rev2](MidiMessage const &message) {
			auto newPatch = rev2.patchPolySequenceToGatedTrack(message, 0);
			controller->getMidiOutput(rev2.midiOutput())->sendMessageNow(newPatch);
			logger->postMessage("Copied the poly sequence to gated track #1");
			handler->removeEditBufferHandler(handle);
		});
		controller->getMidiOutput(rev2.midiOutput())->sendMessageNow(rev2.requestEditBufferDump());
	} } },
	{ "clearPolyA",{ "Clear Poly Layer A", [this, controller, handler, logger, &rev2]() {
		auto handle = EditBufferHandler::makeOne();
		handler->setNextEditBufferHandler(handle, [this, handle, controller, handler, logger, &rev2](MidiMessage const &message) {
			controller->getMidiOutput(rev2.midiOutput())->sendMessageNow(rev2.clearPolySequencer(message, true, false));
			logger->postMessage("Cleared poly sequence on Layer A");
			handler->removeEditBufferHandler(handle);
		});
		controller->getMidiOutput(rev2.midiOutput())->sendMessageNow(rev2.requestEditBufferDump());
	} } },
	{ "clearPolyB",{ "Clear Poly Layer B", [this, controller, handler, logger, &rev2]() {
		auto handle = EditBufferHandler::makeOne();
		handler->setNextEditBufferHandler(handle, [this, handle, handler, controller, logger, &rev2](MidiMessage const &message) {
			controller->getMidiOutput(rev2.midiOutput())->sendMessageNow(rev2.clearPolySequencer(message, false, true));
			logger->postMessage("Cleared poly sequence on Layer B");
			handler->removeEditBufferHandler(handle);
		});
		controller->getMidiOutput(rev2.midiOutput())->sendMessageNow(rev2.requestEditBufferDump());
	} } },
	{ "clearPolyBoth",{ "Clear Poly Both", [this, controller, handler, logger, &rev2]() {
		auto handle = EditBufferHandler::makeOne();
		handler->setNextEditBufferHandler(handle, [this, handle, controller, handler, logger, &rev2](MidiMessage const &message) {
			controller->getMidiOutput(rev2.midiOutput())->sendMessageNow(rev2.clearPolySequencer(message, true, true));
			logger->postMessage("Cleared poly sequence on both Layer A and Layer B");
			handler->removeEditBufferHandler(handle);
		});
		controller->getMidiOutput(rev2.midiOutput())->sendMessageNow(rev2.requestEditBufferDump());
	} } },
	{ "makeSeqPersist",{ "Lock Poly and Gated sequence", [this, controller, handler, logger, &rev2]() {
		auto handle = EditBufferHandler::makeOne();
		handler->setNextEditBufferHandler(handle, [this, handle, handler, logger, &rev2](MidiMessage const &message) {
			lockedSequence_ = message;
			logger->postMessage("Retrieved sequences from current program and locked them.");
			isLocked_ = true;
			handler->removeEditBufferHandler(handle);
		});
		controller->getMidiOutput(rev2.midiOutput())->sendMessageNow(rev2.requestEditBufferDump());
	} } }
	};
	setButtonDefinitions(buttonDefs);

	// And we need one more midi handler that will stay for the whole runtime of the program, reacting only on program change messages from the Rev2
	programChangeHandle_ = EditBufferHandler::makeOne();
	handler->setNextEditBufferHandler(programChangeHandle_, [this, handler, &rev2, controller, logger](MidiMessage const &message) {
		if (message.isProgramChange()) {
			if (isLocked_) {
				// New feature: make the poly and gated sequences survive a program change by patching them immediately back in!
				auto handle = EditBufferHandler::makeOne();
				handler->setNextEditBufferHandler(handle, [this, handle, &rev2, handler, controller, logger](MidiMessage const &message) {
					if (rev2.isEditBufferDump(message)) {
						currentPatch_ = rev2.patchFromSysex(message);
						auto patchedBack = rev2.copySequencersFromOther(message, lockedSequence_);
						controller->getMidiOutput(rev2.midiOutput())->sendMessageNow(patchedBack);
						logger->postMessage("Program change - Restored all sequences from locked data");
						handler->removeEditBufferHandler(handle);
					}
				});
				controller->enableMidiInput(rev2.midiInput());
				controller->getMidiOutput(rev2.midiOutput())->sendMessageNow(rev2.requestEditBufferDump());
			}
			else {
				auto handle = EditBufferHandler::makeOne();
				handler->setNextEditBufferHandler(handle, [this, handle, handler, logger, &rev2](MidiMessage const &message) {
					if (rev2.isEditBufferDump(message)) {
						currentPatch_ = rev2.patchFromSysex(message);
						logger->postMessage("Retrieved new patch after program change");
						handler->removeEditBufferHandler(handle);
					}
				});
				controller->getMidiOutput(rev2.midiOutput())->sendMessageNow(rev2.requestEditBufferDump());
			}
		}
	});
}

Rev2ButtonStrip::~Rev2ButtonStrip() {
	handler_->removeEditBufferHandler(programChangeHandle_);
}

