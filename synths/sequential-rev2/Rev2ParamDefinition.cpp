#include "Rev2ParamDefinition.h"

#include "Capability.h"
#include "Patch.h"

#include "MidiHelpers.h"

namespace midikraft {

	const int kSysexStartLayerB = 1024; // Layer B starts at sysex index 1024
	const int kNRPNStartLayerB = 2048; // The NRPN numbers for layer B start 2048 higher than those for layer A

	Rev2ParamDefinition::Rev2ParamDefinition(int number, int min, int max, std::string const &name, int sysExIndex) :
		type_(ParamType::INT), targetLayer_(0), sourceLayer_(0), number_(number), min_(min), max_(max), name_(name), endNumber_(number), sysex_(sysExIndex)
	{
	}

	Rev2ParamDefinition::Rev2ParamDefinition(int startNumber, int endNumber, int min, int max, std::string const &name, int sysExIndex) :
		Rev2ParamDefinition(startNumber, min, max, name, sysExIndex)
	{
		type_ = SynthParameterDefinition::ParamType::INT_ARRAY;
		endNumber_ = endNumber;
	}

	Rev2ParamDefinition::Rev2ParamDefinition(int number, int min, int max, std::string const &name, int sysExIndex, std::map<int, std::string> const &valueLookup) :
		Rev2ParamDefinition(number, min, max, name, sysExIndex)
	{
		type_ = SynthParameterDefinition::ParamType::LOOKUP;
		// Special case
		lookupFunction_ = [valueLookup](int value) { 	
			if (valueLookup.find(value) != valueLookup.end()) {
				return valueLookup.at(value);
			}
			return std::string("unknown"); 
		};
	}

	Rev2ParamDefinition::Rev2ParamDefinition(int number, int min, int max, std::string const &name, int sysExIndex, std::function<std::string(int)> &lookupFunction) :
		Rev2ParamDefinition(number, min, max, name, sysExIndex)
	{
		type_ = SynthParameterDefinition::ParamType::LOOKUP;
		lookupFunction_ = lookupFunction;
	}

	Rev2ParamDefinition::Rev2ParamDefinition(int startNumber, int endNumber, int min, int max, std::string const &name, int sysExIndex, std::map<int, std::string> const &valueLookup) :
		Rev2ParamDefinition(startNumber, min, max, name, sysExIndex, valueLookup)
	{
		type_ = SynthParameterDefinition::ParamType::LOOKUP_ARRAY;
		endNumber_ = endNumber;
	}

	Rev2ParamDefinition::Rev2ParamDefinition(int startNumber, int endNumber, int min, int max, std::string const &name, int sysExIndex, std::function<std::string(int)> &lookupFunction) :
		Rev2ParamDefinition(startNumber, min, max, name, sysExIndex, lookupFunction)
	{
		type_ = SynthParameterDefinition::ParamType::LOOKUP_ARRAY;
		endNumber_ = endNumber;
	}

	std::string Rev2ParamDefinition::name() const
	{
		return name_;
	}

	int Rev2ParamDefinition::sysexIndex() const
	{
		jassert(targetLayer_ == 0 || targetLayer_ == 1);
		return sysex_ + (targetLayer_ == 1 ? kSysexStartLayerB : 0);
	}

	int Rev2ParamDefinition::readSysexIndex() const
	{
		jassert(sourceLayer_ == 0 || sourceLayer_ == 1);
		return sysex_ + (sourceLayer_ == 1 ? kSysexStartLayerB : 0);
	}

	int Rev2ParamDefinition::endSysexIndex() const
	{
		// This is allowed because parameters with consecutive NRPN controller numbers are stored consecutively in the 
		// sysex as well.
		return sysexIndex() + endNumber_ - number_;
	}

	int Rev2ParamDefinition::readEndSysexIndex() const
	{
		// This is allowed because parameters with consecutive NRPN controller numbers are stored consecutively in the 
		// sysex as well.
		return readSysexIndex() + endNumber_ - number_;
	}


	std::string Rev2ParamDefinition::description() const
	{
		return name_;
	}

	int Rev2ParamDefinition::minValue() const
	{
		return min_;
	}

	int Rev2ParamDefinition::maxValue() const
	{
		return max_;
	}

	bool Rev2ParamDefinition::valueInPatch(DataFile const &patch, int &outValue) const
	{
		outValue = patch.at(readSysexIndex());
		return true;
	}

	bool Rev2ParamDefinition::valueInPatch(DataFile const &patch, std::vector<int> &outValue) const
	{
		// If this is not an array type, that won't work
		if (type() != SynthParameterDefinition::ParamType::INT_ARRAY && type() != SynthParameterDefinition::ParamType::LOOKUP_ARRAY) {
			return false;
		}

		outValue.clear();
		for (int i = readSysexIndex(); i <= readEndSysexIndex(); i++) {
			outValue.push_back(patch.at(i));
		}

		return true;
	}

	std::vector<MidiMessage> Rev2ParamDefinition::setValueMessages(std::shared_ptr<DataFile> const patch, Synth const *synth) const
	{
		auto midiLocation = midikraft::Capability::hasCapability<MidiLocationCapability const>(synth);
		if (midiLocation) {
			int nrpnNumberToUse = number_ + (targetLayer_ == 1 ? kNRPNStartLayerB : 0);
			switch (type()) {
			case SynthParameterDefinition::ParamType::LOOKUP:
				// Fall through
			case SynthParameterDefinition::ParamType::INT: {
				int value;
				if (valueInPatch(*patch, value)) {
					return MidiHelpers::generateRPN(midiLocation->channel().toOneBasedInt(), nrpnNumberToUse, value, true, true, true);
				}
			}
			case SynthParameterDefinition::ParamType::LOOKUP_ARRAY:
				// Fall through
			case SynthParameterDefinition::ParamType::INT_ARRAY: {
				std::vector<MidiMessage> result;
				std::vector<int> values;
				if (valueInPatch(*patch, values)) {
					int idx = 0;
					for (auto value : values) {
						auto buffer = MidiHelpers::generateRPN(midiLocation->channel().toOneBasedInt(), nrpnNumberToUse + idx, value, true, true, true);
						std::copy(buffer.cbegin(), buffer.cend(), std::back_inserter(result));
						idx++;
					}
					return result;
				}
			}
			}
		}
		return {};
	}

	void Rev2ParamDefinition::setTargetLayer(int layerNo)
	{
		jassert(layerNo == 0 || layerNo == 1);
		targetLayer_ = layerNo == 1 ? 1 : 0; // Use only 1 or 0, and 0 is default in case of invalid parameter
	}

	int Rev2ParamDefinition::getTargetLayer() const
	{
		return targetLayer_;
	}

	void Rev2ParamDefinition::setSourceLayer(int layerNo)
	{
		jassert(layerNo == 0 || layerNo == 1);
		sourceLayer_ = layerNo;
	}

	int Rev2ParamDefinition::getSourceLayer() const
	{
		return sourceLayer_;
	}

	std::string Rev2ParamDefinition::valueInPatchToText(DataFile const &patch) const
	{
		switch (type()) {
		case SynthParameterDefinition::ParamType::INT: {
			int value;
			if (valueInPatch(patch, value)) {
				return String(value).toStdString();
			}
			return "invalid param";
		}
		case SynthParameterDefinition::ParamType::LOOKUP_ARRAY:
			// Fall through
		case SynthParameterDefinition::ParamType::INT_ARRAY: {
			std::vector<int> value;
			if (valueInPatch(patch, value)) {
				std::stringstream result;
				result << "[";
				for (size_t i = 0; i < value.size(); i++) {
					if (type() == SynthParameterDefinition::ParamType::INT_ARRAY) {
						result << String(value[i]);
					}
					else {
						result << "'" << lookupFunction_(value[i]) << "'";
					}
					if (i != value.size() - 1) result << ", ";
				}
				result << "]";
				return result.str();
			}
			return "invalid vector param";
		}
		case SynthParameterDefinition::ParamType::LOOKUP:
			int value;
			if (valueInPatch(patch, value)) {
				return lookupFunction_(value);
			}
			return "invalid lookup param";
		}
		return "invalid param type";
	}

	void Rev2ParamDefinition::setInPatch(DataFile &patch, int value) const
	{
		jassert(type() == SynthParameterDefinition::ParamType::INT);
		patch.setAt(sysexIndex(), (uint8)value);
	}

	void Rev2ParamDefinition::setInPatch(DataFile &patch, std::vector<int> value) const
	{
		int read = 0;
		for (int i = sysexIndex(); i <= endSysexIndex(); i++) {
			if (read < (int) value.size()) {
				patch.setAt(i, (uint8)value[read++]);
			}
			else {
				// We just ignore additional bytes specified
			}
		}
	}

	midikraft::SynthParameterDefinition::ParamType Rev2ParamDefinition::type() const
	{
		return type_;
	}

}

