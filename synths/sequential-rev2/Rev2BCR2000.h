#pragma once

#include "BCRDefinition.h"

class Rev2;

#include <vector>

class Rev2BCR2000Definition : public BCRStandardDefinition {
public:
	Rev2BCR2000Definition(BCRtype type, int number, int nrpn, bool canBeCloned = true, BCRledMode ledMode = ONE_DOT_OFF) : 
		BCRStandardDefinition(type, number), nrpn_(nrpn), canBeCloned_(canBeCloned), ledMode_(ledMode) {}

	virtual Rev2BCR2000Definition *cloneWithOffset(int offset, int additionalNRPNOffset);
	virtual std::string generateBCR(int channel) const;

	bool canBeCloned() const { return canBeCloned_;  }

protected: 
	int nrpn_;
	bool canBeCloned_;
	BCRledMode ledMode_;
};

class Rev2BCR2000RestDefinition : public Rev2BCR2000Definition {
public:
	Rev2BCR2000RestDefinition(int number, int nrpn) : Rev2BCR2000Definition(BUTTON, number, nrpn) {}

	virtual Rev2BCR2000Definition *cloneWithOffset(int offset, int additionalNRPNOffset) override;

	// This is the super special case of the REST buttons for the Gated Sequencer
	// in order for the lamp to show "active" = 0, we need to invert the value
	virtual std::string generateBCR(int channel) const override;
};

class Rev2BCR2000 {
public:
	static std::string generateBCR(Rev2 &rev2, int baseStoragePlace, bool includeHeaderAndFooter = true);

private:
	static std::vector<Rev2BCR2000Definition *> explodeBy8(std::vector<Rev2BCR2000Definition *> const &controllerSetup, int nrpnOffset = 0);
	static std::string generateMapping(std::string const &presetName, int midiChannel, int storagePlace, int additionalNRPNOffset);
};

