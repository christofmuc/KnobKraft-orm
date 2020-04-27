#pragma once

#include "SimpleDiscoverableDevice.h"
#include "Synth.h"

class RefaceDXDiscovery : public Synth /* TODO should actually only derive from SimpleDiscoverableDevice */ {
public:
	virtual MidiMessage deviceDetect(int channel) override;
	virtual int deviceDetectSleepMS() override;
	virtual MidiChannel channelIfValidDeviceResponse(const MidiMessage &message) override;
	virtual bool needsChannelSpecificDetection() override;

protected:
	uint8 deviceID_ = 0x00; // Useful for?
};
