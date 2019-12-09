#include "Rev2ParamDefinition.h"

#include "Patch.h"

namespace midikraft {

	Rev2ParamDefinition::Rev2ParamDefinition(NrpnDefinition const &nrpn) : nrpn_(nrpn)
	{
	}

	std::string Rev2ParamDefinition::name() const
	{
		return nrpn_.name();
	}

	std::string Rev2ParamDefinition::valueAsText(int value) const
	{
		return nrpn_.valueAsText(value);
	}

	int Rev2ParamDefinition::sysexIndex() const
	{
		return nrpn_.sysexIndex();
	}

	int Rev2ParamDefinition::endSysexIndex() const
	{
		return nrpn_.sysexIndex() + nrpn_.numberOfValues() - 1;
	}

	std::string Rev2ParamDefinition::description() const
	{
		return nrpn_.name();
	}

	int Rev2ParamDefinition::minValue() const
	{
		return nrpn_.min();
	}

	int Rev2ParamDefinition::maxValue() const
	{
		return nrpn_.max();
	}

	bool Rev2ParamDefinition::valueInPatch(Patch const &patch, int &outValue) const
	{
		outValue = patch.at(sysexIndex());
		return true;
	}

	bool Rev2ParamDefinition::valueInPatch(Patch const &patch, std::vector<int> &outValue) const
	{
		if (type() != SynthParameterDefinition::ParamType::INT_ARRAY) {
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
				return MidiRPNGenerator::generate(synth->channel().toOneBasedInt(), nrpn_.number(), value, true);
			}
		}
		case SynthParameterDefinition::ParamType::INT_ARRAY: {
			MidiBuffer result;
			std::vector<int> values;
			if (valueInPatch(patch, values)) {
				for (auto value : values) {
					auto buffer = MidiRPNGenerator::generate(synth->channel().toOneBasedInt(), nrpn_.number(), value, true);
					result.addEvents(buffer, 0, -1, 0);
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
		case SynthParameterDefinition::ParamType::INT_ARRAY: {
			std::vector<int> value;
			if (valueInPatch(patch, value)) {
				std::stringstream result;
				result << "[";
				for (int i = 0; i < value.size(); i++) {
					result << String(value[i]);
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
				return nrpn_.valueAsText(value);
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
		if (sysexIndex() != endSysexIndex()) {
			return SynthParameterDefinition::ParamType::INT_ARRAY;
		}
		else if (nrpn_.isLookup()) {
			return SynthParameterDefinition::ParamType::LOOKUP;
		}
		return SynthParameterDefinition::ParamType::INT;
	}

}

