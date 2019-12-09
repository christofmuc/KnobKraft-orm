#pragma once

#include "SynthParameterDefinition.h"

namespace midikraft {

	class Rev2ParamDefinition : public SynthParameterDefinition, 
		public SynthVectorParameterCapability, public SynthLookupParameterCapability, public SynthParameterLiveEditCapability {
	public:
		Rev2ParamDefinition(int number, int min, int max, std::string const &name, int sysExIndex);
		Rev2ParamDefinition(int number, int min, int max, std::string const &name, int sysExIndex, std::map<int, std::string> const &valueLookup);
		Rev2ParamDefinition(int startNumber, int endNumber, int min, int max, std::string const &name, int sysExIndex);

		//! The parameter definition is a meta-data struct only, it is totally ok to copy these around
		Rev2ParamDefinition(Rev2ParamDefinition const &other) = default; 

		virtual ParamType type() const override;
		virtual std::string name() const override;
		virtual std::string description() const override;
		virtual std::string valueInPatchToText(Patch const &patch) const override;

		// SynthIntParameterCapability
		virtual int minValue() const override;
		virtual int maxValue() const override;
		virtual int sysexIndex() const override;
		virtual bool valueInPatch(Patch const &patch, int &outValue) const override;
		virtual void setInPatch(Patch &patch, int value) const override;

		// SynthVectorParameterCapability
		virtual int endSysexIndex() const override;
		virtual bool valueInPatch(Patch const &patch, std::vector<int> &outValue) const override;
		virtual void setInPatch(Patch &patch, std::vector<int> value) const override;

		// SynthLookupCapability
		virtual std::string valueAsText(int value) const override;

		// SynthParameterLiveEditCapability
		virtual MidiBuffer setValueMessages(Patch const &patch, Synth *synth) const override;

	private:
		std::string lookupValueAsText(int value) const;

		int number_;
		int endNumber_;
		int min_;
		int max_;
		int sysex_;
		std::string name_;
		std::map<int, std::string> valueLookup_;
	};

}
