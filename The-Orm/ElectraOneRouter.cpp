/*
   Copyright (c) 2021 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "ElectraOneRouter.h"

#include "UIModel.h"

#include "Logger.h"

ElectraOneRouter::ElectraOneRouter() : enabled_(false)
{
}

ElectraOneRouter::~ElectraOneRouter()
{
	// Delete message handler, in case it was created
	if (!routerCallback_.isNull()) {
		midikraft::MidiController::instance()->removeMessageHandler(routerCallback_);
	}
}

void ElectraOneRouter::enable(bool enabled)
{
	if (enabled) {
		enabled_ = true;
		if (routerCallback_.isNull()) {
			// Install handler
			routerCallback_ = midikraft::MidiController::makeOneHandle();
			midikraft::MidiController::instance()->addMessageHandler(routerCallback_, [](MidiInput *source, MidiMessage const &message) {
				if (source->getName() == "Electra Controller") {
					auto toWhichSynthToForward = UIModel::currentSynth();
					if (toWhichSynthToForward) {
						auto location = midikraft::Capability::hasCapability<midikraft::MidiLocationCapability>(toWhichSynthToForward);
						if (location) {
							// Check if this is a channel message, and if yes, re-channel to the current synth
							MidiMessage channelMessage = message;
							if (message.getChannel() != 0) {
								channelMessage.setChannel(location->channel().toOneBasedInt());
							}
							midikraft::MidiController::instance()->getMidiOutput(location->midiOutput())->sendMessageNow(channelMessage);
						}
					}
				}
			});
		}
		// Make sure to listen to the Electra One if found!
		//midikraft::MidiController::instance()->enableMidiInput("Electra Controller");
		//SimpleLogger::instance()->postMessage("Listening to messages from USB input Electra Controller");
	}
	else {
		// Stop listening!
		enabled_ = false;
		//midikraft::MidiController::instance()->disableMidiInput("Electra Controller");
		//SimpleLogger::instance()->postMessage("Turning off USB input Electra Controller");
	}
}
