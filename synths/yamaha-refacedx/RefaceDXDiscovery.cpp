/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "RefaceDXDiscovery.h"

#include "MidiHelpers.h"

namespace midikraft {

	std::vector<juce::MidiMessage> RefaceDXDiscovery::deviceDetect(int /* channel */)
	{
		return { MidiHelpers::sysexMessage({ 0x7e, (uint8)(0x00 | deviceID_), 0x06, 0x01 }) };
	}

	int RefaceDXDiscovery::deviceDetectSleepMS()
	{
		// Modern and fast
		return 60;
	}

	MidiChannel RefaceDXDiscovery::channelIfValidDeviceResponse(const MidiMessage &message)
	{
		if (message.isSysEx()) {
			// The manual doesn't say why this is the right message, but hey... 
			auto expectedResponse = MidiHelpers::sysexMessage({ 0x7E, 0x7F,  0x06,  0x02,  0x43,  0x00,  0x41,  0x53,  0x06,  0x02,  0x00,  0x00,  0x7F });
			// Compare only the first 9 digits, the rest seems to be the version number which can change of course
			// see http://midi.teragonaudio.com/tech/midispec/identity.htm
			if (MidiHelpers::equalSysexMessageContent(message, expectedResponse, 9)) {
				// Valid, but on which MIDI channel does it receive?
				// I will overload this function in the RefaceDX class itself, because this method does not work for our purpose
				return MidiChannel::fromZeroBase(0);
			}
		}
		return MidiChannel::invalidChannel();
	}

	bool RefaceDXDiscovery::needsChannelSpecificDetection()
	{
		// The manual says this device "receives under omni"
		return false;
	}

}
