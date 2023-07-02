#include "ModelD.h"

ModelD::ModelD(std::string const &deviceName, MidiChannel channel) : deviceName_(deviceName), channel_(channel)
{
}

std::string ModelD::getName() const
{
	return deviceName_;
}

MidiChannel ModelD::getInputChannel() const
{
	return channel_;
}

