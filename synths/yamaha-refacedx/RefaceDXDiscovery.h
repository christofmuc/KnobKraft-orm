/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "SimpleDiscoverableDevice.h"
#include "Synth.h"

namespace midikraft {

	class RefaceDXDiscovery : public SimpleDiscoverableDevice /* TODO should actually only derive from SimpleDiscoverableDevice */ {
	public:
		virtual MidiMessage deviceDetect(int channel) override;
		virtual int deviceDetectSleepMS() override;
		virtual MidiChannel channelIfValidDeviceResponse(const MidiMessage &message) override;
		virtual bool needsChannelSpecificDetection() override;

	protected:
		uint8 deviceID_ = 0x00; // Useful for?
	};

}
