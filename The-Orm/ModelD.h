#pragma once

#include "ReadonlySoundExpander.h"

class ModelD : public midikraft::ReadonlySoundExpander{
public:
	ModelD(std::string const &deviceName, MidiChannel channel);

	virtual std::string getName() const override;
	virtual MidiChannel getInputChannel() const override;

	virtual bool canChangeInputChannel() const {
		return false;
	}

	virtual void changeInputChannel(midikraft::MidiController* , MidiChannel , std::function<void()> )
	{

	}
	virtual bool hasMidiControl() const {
		return false;
	}
	virtual bool isMidiControlOn() const {
		return true;
	}
	virtual void setMidiControl(midikraft::MidiController* , bool ) {

	}


private:
	std::string deviceName_;
	MidiChannel channel_;
};
