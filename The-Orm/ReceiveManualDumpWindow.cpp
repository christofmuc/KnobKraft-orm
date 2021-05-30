/*
   Copyright (c) 2021 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "ReceiveManualDumpWindow.h"

#include "MidiController.h"

#include "Capability.h"

ReceiveManualDumpWindow::ReceiveManualDumpWindow(std::shared_ptr<midikraft::Synth> synth) : 
	ThreadWithProgressWindow("Waiting for sysex messages from " + synth->getName() +"...", false, true, 1000, "Stop"), synth_(synth)
{
	// Create a MIDI log view with a decent size
	midiLog_ = std::make_unique<MidiLogView>(false, true);
	midiLog_->setSize(800, 400);

	// Add this as a custom component to our AlertWindow
	getAlertWindow()->addCustomComponent(midiLog_.get());
}

void ReceiveManualDumpWindow::run()
{
	// Determine which MIDI port to listen to
	auto locationCap = midikraft::Capability::hasCapability<midikraft::MidiLocationCapability>(synth_);
	
	auto incomingHandler = midikraft::MidiController::makeOneHandle();
	midikraft::MidiController::instance()->addMessageHandler(incomingHandler, [this, locationCap](MidiInput *source, MidiMessage const &received) {
		// Just capture all messages incoming from the devices' input port. That might be too many...
		if (!locationCap || locationCap->midiInput() == source->getName()) {
			midiLog_->addMessageToList(received, source->getName(), false);
			receivedMessages_.push_back(received);
		}
	});

	do {
	} while (!threadShouldExit());
	// Don't forget to unregister!
	midikraft::MidiController::instance()->removeMessageHandler(incomingHandler);
	incomingHandler = midikraft::MidiController::makeNoneHandle();
}

std::vector<juce::MidiMessage> ReceiveManualDumpWindow::result() const
{
	return receivedMessages_;
}
