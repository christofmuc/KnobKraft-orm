#pragma once

#include "SynthParameterDefinition.h"

namespace midikraft {

	class Rev2ParamDefinition : public SynthParameterDefinition, 
		public SynthVectorParameterCapability, public SynthParameterLiveEditCapability, public SynthMultiLayerParameterCapability {
	public:
		Rev2ParamDefinition(int number, int min, int max, std::string const &name, int sysExIndex);
		Rev2ParamDefinition(int number, int min, int max, std::string const &name, int sysExIndex, std::map<int, std::string> const &valueLookup);
		Rev2ParamDefinition(int number, int min, int max, std::string const &name, int sysExIndex, std::function<std::string(int)> &lookupFunction);
		Rev2ParamDefinition(int startNumber, int endNumber, int min, int max, std::string const &name, int sysExIndex);
		Rev2ParamDefinition(int startNumber, int endNumber, int min, int max, std::string const &name, int sysExIndex, std::map<int, std::string> const &valueLookup);
		Rev2ParamDefinition(int startNumber, int endNumber, int min, int max, std::string const &name, int sysExIndex, std::function<std::string(int)> &lookupFunction);

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

		// SynthParameterLiveEditCapability
		virtual MidiBuffer setValueMessages(Patch const &patch, Synth const *synth) const override;

		// SynthMultiLayerParameterCapability 
		virtual void setTargetLayer(int layerNo) override;
		virtual int getTargetLayer() const override;

	private:
		ParamType type_;
		int targetLayer_; // The Rev2 has no layers, A (=0) and B (=0). By default, we target 0 but can change this calling setTargetLayer()
		int number_;
		int endNumber_;
		int min_;
		int max_;
		int sysex_;
		std::string name_;
		std::function<std::string(int)> lookupFunction_;
	};

}
