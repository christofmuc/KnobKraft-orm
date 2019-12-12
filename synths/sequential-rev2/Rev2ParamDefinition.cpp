#include "Rev2ParamDefinition.h"

#include "Patch.h"

namespace midikraft {

	Rev2ParamDefinition::Rev2ParamDefinition(int number, int min, int max, std::string const &name, int sysExIndex) :
		type_(ParamType::INT), number_(number), min_(min), max_(max), name_(name), endNumber_(number), sysex_(sysExIndex)
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
		return sysex_;
	}

	int Rev2ParamDefinition::endSysexIndex() const
	{
		// This is allowed because parameters with consecutive NRPN controller numbers are stored consecutively in the 
		// sysex as well.
		return sysex_ + endNumber_ - number_;
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

	bool Rev2ParamDefinition::valueInPatch(Patch const &patch, int &outValue) const
	{
		outValue = patch.at(sysexIndex());
		return true;
	}

	bool Rev2ParamDefinition::valueInPatch(Patch const &patch, std::vector<int> &outValue) const
	{
		// If this is not an array type, that won't work
		if (type() != SynthParameterDefinition::ParamType::INT_ARRAY && type() != SynthParameterDefinition::ParamType::LOOKUP_ARRAY) {
			return false;
		}

		outValue.clear();
		for (int i = sysexIndex(); i <= endSysexIndex(); i++) {
			outValue.push_back(patch.at(i));
		}

		return true;
	}

	MidiBuffer Rev2ParamDefinition::setValueMessages(Patch const &patch, Synth *synth) const
	{
		switch (type()) {
		case SynthParameterDefinition::ParamType::LOOKUP:
			// Fall through
		case SynthParameterDefinition::ParamType::INT: {
			int value;
			if (valueInPatch(patch, value)) {
				return MidiRPNGenerator::generate(synth->channel().toOneBasedInt(), number_, value, true);
			}
		}
		case SynthParameterDefinition::ParamType::LOOKUP_ARRAY:
			// Fall through
		case SynthParameterDefinition::ParamType::INT_ARRAY: {
			MidiBuffer result;
			std::vector<int> values;
			if (valueInPatch(patch, values)) {
				int idx = 0;
				for (auto value : values) {
					auto buffer = MidiRPNGenerator::generate(synth->channel().toOneBasedInt(), number_ + idx, value, true);
					result.addEvents(buffer, 0, -1, 0);
					idx++;
				}
				return result;
			}
		}
		}
		return MidiBuffer();
	}

	std::string Rev2ParamDefinition::valueInPatchToText(Patch const &patch) const
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
				for (int i = 0; i < value.size(); i++) {
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

	void Rev2ParamDefinition::setInPatch(Patch &patch, int value) const
	{
		jassert(type() == SynthParameterDefinition::ParamType::INT);
		patch.setAt(sysexIndex(), (uint8)value);
	}

	void Rev2ParamDefinition::setInPatch(Patch &patch, std::vector<int> value) const
	{
		int read = 0;
		for (int i = sysexIndex(); i <= endSysexIndex(); i++) {
			if (read < value.size()) {
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

