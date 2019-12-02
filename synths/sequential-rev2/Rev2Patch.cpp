/*
   Copyright (c) 2019 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "Rev2Patch.h"

#include "Sysex.h"
#include "Rev2.h"

#include "BinaryResources.h"

#include <boost/format.hpp>
#include <boost/algorithm/string.hpp>

namespace midikraft {

	std::string Rev2PatchNumber::friendlyName() const {
		// The Rev2 has 8 banks of 128 patches, in two sections U and F called U1 to U4 and F1 to F4
		int section = midiProgramNumber().toZeroBased() / 128;
		int program = midiProgramNumber().toZeroBased() % 128;
		return (boost::format("%s%d P%d") % (section == 0 ? "U" : "F") % ((bank_ % 4) + 1) % program).str();
	}

	Rev2Patch::Rev2Patch() : oscAllocated_(1)
	{
		// Load the init patch
		MidiMessage initPatch = MidiMessage::createSysExMessage(Rev2_InitPatch_syx, Rev2_InitPatch_syx_size);

		// Set data
		Rev2 rev2;
		auto initpatch = rev2.patchFromSysex(initPatch);
		setData(initpatch->data());
	}

	Rev2Patch::Rev2Patch(Synth::PatchData const &patchData) : Patch(patchData)
	{
	}

	std::string Rev2Patch::patchName() const
	{
		std::string layerA = layerName(0);
		std::string layerB = layerName(1);
		boost::trim(layerA);
		boost::trim(layerB);

		if (layerA == layerB) {
			switch (layerMode()) {
			case LayeredPatch::SEPARATE: return layerA + " [2x]"; // That's a weird state
			case LayeredPatch::STACK: return layerA + "[+]";
			case LayeredPatch::SPLIT: return layerA + "[|]"; // That's a weird state
			}
		}
		else {
			switch (layerMode()) {
			case LayeredPatch::SEPARATE: return layerA + "." + layerB;
			case LayeredPatch::STACK: return layerA + "[+]";  // return layerA + "+" + layerB;
			case LayeredPatch::SPLIT: return layerA + "|" + layerB;
			}
		}
		return "invalid patch";
	}

	void Rev2Patch::setName(std::string const &name)
	{
		ignoreUnused(name);
		jassert(false);
	}

	std::shared_ptr<PatchNumber> Rev2Patch::patchNumber() const {
		return std::make_shared<Rev2PatchNumber>(number_);
	}

	void Rev2Patch::setPatchNumber(MidiProgramNumber patchNumber) {
		number_ = Rev2PatchNumber(patchNumber);
	}

	int Rev2Patch::value(SynthParameterDefinition const &param) const
	{
		ignoreUnused(param);
		throw std::logic_error("The method or operation is not implemented.");
	}

	SynthParameterDefinition const & Rev2Patch::paramBySysexIndex(int sysexIndex) const
	{
		ignoreUnused(sysexIndex);
		throw std::logic_error("The method or operation is not implemented.");
	}

	void Rev2Patch::addNrpns(std::vector<NRPNValue> const &values)
	{
		// This will poke the values into the patch no matter what!
		for (auto value : values) {
			if (value.def().sysexIndex() != -1) {
				setAt(value.def().sysexIndex(), (uint8)value.value());
			}
			else {
				throw new std::runtime_error("Error - this is not a sysex parameter");
			}
		}
	}

/*	int Rev2Patch::addOsc2Patch(SimpleOsc const &osc)
	{
		std::vector<NRPNValue> result;

		// Set the Shape
		int targetOscNo = oscAllocated_++;
		if (targetOscNo > 2) {
			return targetOscNo;
		}
		jassert(targetOscNo >= 1 && targetOscNo <= 2);
		int shape = 0;
		int shapemod = 0;
		NrpnDefinition oscShape = Rev2::nrpn((boost::format("Osc %d Shape") % targetOscNo).str());
		if (osc.shape() == "Square") {
			// Square is a special case - the Rev2 will make that with Pulse and Shapemod == 50
			shape = oscShape.valueFromText("Pulse");
			shapemod = 50;
		}
		else if (osc.shape() == "Saw") {
			shape = oscShape.valueFromText("Saw");
		}
		else if (osc.shape() == "Triangle") {
			shape = oscShape.valueFromText("Triangle");
		}
		else if (osc.shape() == "Sawwave") {
			shape = oscShape.valueFromText("Saw+Triangle"); // Absolutely not sure if this is correct, need to measure with oscilloscope
			shapemod = osc.shapemod();
		}
		else if (osc.shape() == "Pulse") {
			shape = oscShape.valueFromText("Pulse");
			shapemod = osc.shapemod();
		}
		else if (osc.shape() == "") {
			shape = oscShape.valueFromText("Off");
		}
		else {
			jassert(false);
		}
		result.push_back(NRPNValue(oscShape, shape));
		result.push_back(NRPNValue(Rev2::nrpn((boost::format("Osc %d Pulse Width") % targetOscNo).str()), shapemod));

		// Set the frequency - The Rev2 will set C3 as 0, and semitones up and down from there. 
		result.push_back(NRPNValue(Rev2::nrpn((boost::format("Osc %d Freq") % targetOscNo).str()), 24 + osc.frequency()));

		// Now, to the specials! It is here where it gets interesting
		for (auto special : osc.specials()) {
			if (special->name() == "Noise") {
				if (targetOscNo == 1) {
					// Lucky us, our osc 1 has noise. 
					auto valueParam = std::dynamic_pointer_cast<ValueParameter>(special);
					if (valueParam) {
						// The Noise even is quantified, just as for the Rev
						result.push_back(NRPNValue(Rev2::nrpn("Noise"), valueParam->value()));
					}
					else {
						// Noise seems to be a flag that can just be turned on and off. Wild guess on how much noise you want
						result.push_back(NRPNValue(Rev2::nrpn("Noise"), 50));
					}
					if (targetOscNo != 1) {
						warnings_.push_back((boost::format("Can't turn on noise for oscillator %d, turned it on for Osc1") % targetOscNo).str());
					}
				}
			}
			else if (special->name() == "Detune") {
				auto valueParam = std::dynamic_pointer_cast<ValueParameter>(special);
				if (valueParam) {
					// That's easily possible with our Rev2
					result.push_back(NRPNValue(Rev2::nrpn((boost::format("Osc %d Freq Fine") % targetOscNo).str()), valueParam->value() * 50 / 32 + 50));
				}
				else {
					warnings_.push_back("Found Detune without value, ignoring that");
				}
			}
			else if (special->name() == "Click") {
				warnings_.push_back("Rev2 doesn't support click feature on oscillators");
			}
			else {
				warnings_.push_back((boost::format("Unknown special '%s' ignored") % special->name()).str());
			}
		}

		// Done, we collected all Nrpns we want to set in the patch for this Oscillator, so patch it into the patch!
		addNrpns(result);

		return targetOscNo;
	}

	void Rev2Patch::addEnv2Patch(Envelope const &env, int targetEnvNo)
	{
		std::vector<NRPNValue> result;

		std::string envName;
		switch (targetEnvNo) {
		case 0:
			envName = "VCA Env";
			break;
		case 1:
			envName = "Filter Env";
			break;
		case 2:
			envName = "Env 3";
			break;
		default:
			warnings_.push_back((boost::format("Ignoring definition for additional envelope #%d") % targetEnvNo).str());
			return;
		}

		// Ok, apart from the calibration, this should be straight forward?
		result.push_back(NRPNValue(Rev2::nrpn(envName + " Delay"), roundToInt(env.delay().percentage() * 127)));
		result.push_back(NRPNValue(Rev2::nrpn(envName + " Attack"), roundToInt(env.attack().percentage() * 127)));
		result.push_back(NRPNValue(Rev2::nrpn(envName + " Decay"), roundToInt(env.decay().percentage() * 127)));
		result.push_back(NRPNValue(Rev2::nrpn(envName + " Sustain"), roundToInt(env.sustain().percentage() * 127)));
		result.push_back(NRPNValue(Rev2::nrpn(envName + " Release"), roundToInt(env.release().percentage() * 127)));

		addNrpns(result);
	}*/

	std::vector<SynthParameterDefinition *> Rev2Patch::allParameterDefinitions()
	{
		return std::vector<SynthParameterDefinition *>();
	}

	LayeredPatch::LayerMode Rev2Patch::layerMode() const
	{
		//TODO - the sysex index should be taken from the Rev2 Parameter definitions
		switch (at(231)) {
		case 0: return LayeredPatch::SEPARATE;
		case 1: return LayeredPatch::STACK;
		case 2: return LayeredPatch::SPLIT;
		}
		throw std::runtime_error("Invalid layer mode of Rev2");
	}

	int Rev2Patch::numberOfLayers() const
	{
		// The Rev2 always has 2 layers, one might be an init patch, but we wouldn't know?
		return 2;
	}

	std::string Rev2Patch::layerName(int layerNo) const
	{
		// The Rev2 has a 20 character patch name storage for each of the 2 layers...	
		jassert(layerNo >= 0 && layerNo < numberOfLayers());
		size_t baseIndex = layerNo == 0 ? 235 : 1259; // Layer A starts at 235, Layer B starts at 1259
		std::string layerName;
		for (size_t i = baseIndex; i < baseIndex + 20; i++) {
			layerName.push_back(data()[i]);
		}
		return layerName;
	}

	std::vector<std::string> Rev2Patch::warnings()
	{
		return warnings_;
	}

}
