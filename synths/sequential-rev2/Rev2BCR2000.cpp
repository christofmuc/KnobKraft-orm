#include "Rev2BCR2000.h"

#include "NrpnDefinition.h"
#include "Rev2.h"

#include "BCR2000.h"
#include "NRPNBCRDefinition.h"

#include <vector>
#include <numeric>
#include <boost/format.hpp>

// Trying to squeeze everything into a single BCR2000 preset, for Layer A
std::vector<Rev2BCR2000Definition *> singlePageControllerSetup =
{
	new Rev2BCR2000Definition(ENCODER, 1, 192),
	new Rev2BCR2000Definition(ENCODER, 9, 208),
	new Rev2BCR2000Definition(ENCODER, 17, 224),
	new Rev2BCR2000Definition(ENCODER, 25, 240),
	new Rev2BCR2000RestDefinition(33, 192),
	new Rev2BCR2000RestDefinition(41, 200),
	new Rev2BCR2000Definition(ENCODER, 33, 200),
	new Rev2BCR2000Definition(ENCODER, 41, 216),
	new Rev2BCR2000Definition(ENCODER, 49, 232),
	// new Rev2BCR2000Definition(ENCODER, 57, 248), // Darn, the BCR2000 does not have enough controllers! I need 8 more!
};

// Setup definition of BCR2000 for the Rev2 Gated Step Sequencers.
// 8 Pages for the 8 tracks
std::vector<std::vector<Rev2BCR2000Definition *>> basisControllerSetup = {
	{
		new Rev2BCR2000Definition(ENCODER, 1, 184, false), // Track 1 Destination
		new Rev2BCR2000Definition(ENCODER, 8, 182, false, CUT), // Gated Sequencer Mode
		new Rev2BCR2000Definition(BUTTON, 8, 183, false), // Gated Sequencer On/Off
		new Rev2BCR2000Definition(ENCODER, 33, 192), // Layer A Track 1
		new Rev2BCR2000Definition(ENCODER, 41, 200),
		new Rev2BCR2000RestDefinition(33, 192), // Rest Buttons
		new Rev2BCR2000RestDefinition(41, 200),
	},
	{
		new Rev2BCR2000Definition(ENCODER, 1, 185, false), // Track 2 Destination
		new Rev2BCR2000Definition(ENCODER, 8, 182, false, CUT), // Gated Sequencer Mode
		new Rev2BCR2000Definition(BUTTON, 8, 183, false), // Gated Sequencer On/Off
		new Rev2BCR2000Definition(ENCODER, 33, 208), // Layer A Track 2
		new Rev2BCR2000Definition(ENCODER, 41, 216),
	},
	{
		new Rev2BCR2000Definition(ENCODER, 1, 186, false), // Track 3 Destination
		new Rev2BCR2000Definition(ENCODER, 8, 182, false, CUT), // Gated Sequencer Mode
		new Rev2BCR2000Definition(BUTTON, 8, 183, false), // Gated Sequencer On/Off
		new Rev2BCR2000Definition(ENCODER, 33, 224), // Layer A Track 3
		new Rev2BCR2000Definition(ENCODER, 41, 232),
	},
	{
		new Rev2BCR2000Definition(ENCODER, 1, 187, false), // Track 4 Destination
		new Rev2BCR2000Definition(ENCODER, 8, 182, false, CUT), // Gated Sequencer Mode
		new Rev2BCR2000Definition(BUTTON, 8, 183, false), // Gated Sequencer On/Off
		new Rev2BCR2000Definition(ENCODER, 33, 240), // Layer A Track 4
		new Rev2BCR2000Definition(ENCODER, 41, 248),
	},
};

Rev2BCR2000Definition * Rev2BCR2000Definition::cloneWithOffset(int offset, int additionalNRPNOffset)
{
	return new Rev2BCR2000Definition(type_, number_ + offset, nrpn_ + offset + additionalNRPNOffset);
}

std::string generateBCR_Custom(NRPNBCRdefinition const &def, int channel) 
{
	int code = 0xB0 | channel;
	std::string nrpnString = (boost::format("$%0x $63 $%0x $%0x $62 $%0x $%0x $06 $00 $%0x $26 val")
		% code % (def.nrpnNumber_ >> 7)
		% code % (def.nrpnNumber_ & 0x7f)
		% code
		% code
		).str();

	int fakeChannel = 16;
	switch (def.type()) {
	case ENCODER: {
		// Super special case for the gated sequencer - if the maxValue is 127, limit it to 126
		// The 127 can only be set with the rest button
		int gatedMaxValue = def.maxValue_ == 127 ? 126 : def.maxValue_;
		return (boost::format(
			"$encoder %d ; %s\n"
			"  .easypar NRPN %d %d 1 0 absolute\n"
			"  .tx %s\n"
			"  .minmax %d %d\n"
			"  .default %d\n"
			"  .mode %s\n"
			"  .showvalue on\n"
			"  .resolution 64 92 127 127\n"
		) % def.encoderNumber() % def.description_ % fakeChannel % def.nrpnNumber_ % nrpnString % def.minValue_ % gatedMaxValue % def.defaultValue_ % def.ledMode_).str();
	}
	case BUTTON:
		return (boost::format(
			"$button %d ; %s\n"
			"  .easypar NRPN %d %d %d %d toggleon\n"
			"  .tx %s\n"
			"  .default %s\n"
			"  .showvalue on\n"
		) % def.encoderNumber() % def.description_ % (channel + 1) % def.nrpnNumber_ %  def.maxValue_ % def.minValue_% nrpnString % def.defaultValue_).str(); // Note the flipped min and max for buttons!!!
	default:
		assert(false);
		return (boost::format(
			"; %s\n"
		) % def.description_).str();
	}
}


std::string Rev2BCR2000Definition::generateBCR(int channel) const
{
	// Get the full definition
	auto nrpn = Rev2::nrpn(nrpn_);

	// This works because all parameters in the Rev2 can be set via NRPN numbers.
	auto name = nrpn.name();
	NRPNBCRdefinition def(name, type_, number_, nrpn_, nrpn.min(), nrpn.max(), 0, BCRdefinition::ledMode(ledMode_));
	return generateBCR_Custom(def, channel);
}

std::vector<Rev2BCR2000Definition *> Rev2BCR2000::explodeBy8(std::vector<Rev2BCR2000Definition *> const &controllerSetup, int nrpnOffset) {
	std::vector<Rev2BCR2000Definition *> result;
	for (auto def : controllerSetup) {
		if (def->canBeCloned()) {
			for (int i = 0; i < 8; i++) {
				result.push_back(def->cloneWithOffset(i, nrpnOffset));
			}
		}
		else {
			// Just take it once as is
			result.push_back(def->cloneWithOffset(0, nrpnOffset));
		}
	}
	return result;
}

std::string Rev2BCR2000::generateBCR(Rev2 &rev2, int baseStoragePlace, bool includeHeaderAndFooter)
{
	std::string result;
	if (includeHeaderAndFooter) result = BCR2000::generateBCRHeader();
	assert(baseStoragePlace != -1); // That won't work anymore
	result += generateMapping(rev2.presetName(), rev2.channel().toZeroBasedInt(), baseStoragePlace, 0);
	result += generateMapping(rev2.presetName() + " Layer B", rev2.channel().toZeroBasedInt(), baseStoragePlace + 4, 2048);
	if (includeHeaderAndFooter) result += BCR2000::generateBCREnd(baseStoragePlace);
	return result;
}

std::string Rev2BCR2000::generateMapping(std::string const &presetName, int channel, int storagePlace, int additionalNRPNOffset) {
	std::string result;

	// We're generating a bunch of presets for the BCR2000 now
	for (int presetNum = 0; presetNum < basisControllerSetup.size(); presetNum++) {
		// Postfix the given name with the number
		std::string detailedName = presetName;
		if (presetNum > 0) {
			detailedName = (boost::format("%s %d") % presetName % presetNum).str();
		}
		result += BCR2000::generatePresetHeader(detailedName);

		// Loop over all Matrix 1000 Parameters, and write out a proper encoder definition
		std::vector<std::pair<BCRdefinition *, std::string>> all_entries;
		std::vector<Rev2BCR2000Definition *> controllerSetup = explodeBy8(basisControllerSetup[presetNum], additionalNRPNOffset);
		for (auto controller : controllerSetup) {
			all_entries.push_back(std::make_pair(controller, controller->generateBCR(channel)));
		}

		result += BCR2000::generateAllEncoders(all_entries);
		result += BCR2000::generateBCRFooter(storagePlace != -1 ? storagePlace + presetNum : -1);

		for (auto def : controllerSetup) {
			delete def;
		}
	}

	return result;
}

Rev2BCR2000Definition * Rev2BCR2000RestDefinition::cloneWithOffset(int offset, int additionalNRPNOffset)
{
	return new Rev2BCR2000RestDefinition(number_ + offset, nrpn_ + offset+ additionalNRPNOffset);
}

std::string Rev2BCR2000RestDefinition::generateBCR(int channel) const
{
	// Get the full definition
	auto nrpn = Rev2::nrpn(nrpn_);

	int code = 0xB0 | channel;
	std::vector<int> nrpnSequence = { code, 0x63, nrpn_ >> 7, code, 0x62, (nrpn_ & 0x7f), code, 0x06, 0x00, code, 0x26 };
	int magicNumber = (0x00 - 0xf7 - std::accumulate(nrpnSequence.begin(), nrpnSequence.end(), 0)) & 0x7f;
	std::string nrpnString;
	for (auto codeInSequence : nrpnSequence) {
		nrpnString += (boost::format("$%0x ") % codeInSequence).str();
	}

	int fakeChannel = 16;
	return (boost::format(
		"$button %d ; %s\n"
		"  .easypar NRPN %d %d 1 0 toggleon\n"
		"  .default 1\n"
		"  .mode toggle\n"
		"  .showvalue on\n"
		"  .tx $F0 $7D $7F val cks-2 2 %d $F7 %scks-2 4\n"
	) % number_ % nrpn.name()
		% fakeChannel % nrpn_ % magicNumber % nrpnString).str();

}
