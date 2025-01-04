#pragma once

#include "SynthParameterDefinition.h"

namespace midikraft {

	class Rev2ParamDefinition : public SynthParameterDefinition, 
		public SynthIntParameterCapability, public SynthVectorParameterCapability, public SynthParameterLiveEditCapability, public SynthMultiLayerParameterCapability {
	public:
		Rev2ParamDefinition(int number, int min, int max, std::string const &name, int sysExIndex);
		Rev2ParamDefinition(int number, int min, int max, std::string const &name, int sysExIndex, std::map<int, std::string> const &valueLookup);
		Rev2ParamDefinition(int number, int min, int max, std::string const &name, int sysExIndex, std::function<std::string(int)> &lookupFunction);
		Rev2ParamDefinition(int startNumber, int endNumber, int min, int max, std::string const &name, int sysExIndex);
		Rev2ParamDefinition(int startNumber, int endNumber, int min, int max, std::string const &name, int sysExIndex, std::map<int, std::string> const &valueLookup);
		Rev2ParamDefinition(int startNumber, int endNumber, int min, int max, std::string const &name, int sysExIndex, std::function<std::string(int)> &lookupFunction);

        virtual ~Rev2ParamDefinition() = default;

		//! The parameter definition is a meta-data struct only, it is totally ok to copy these around
		Rev2ParamDefinition(Rev2ParamDefinition const &other) = default; 

		virtual ParamType type() const override;
		virtual std::string name() const override;
		virtual std::string description() const override;
		virtual std::string valueInPatchToText(DataFile const &patch) const override;

		// SynthIntParameterCapability
		virtual int minValue() const override;
		virtual int maxValue() const override;
		virtual int sysexIndex() const override;
		virtual bool valueInPatch(DataFile const &patch, int &outValue) const override;
		virtual void setInPatch(DataFile &patch, int value) const override;
		int readSysexIndex() const;

		// SynthVectorParameterCapability
		virtual int endSysexIndex() const override;
		int readEndSysexIndex() const;
	
		virtual bool valueInPatch(DataFile const &patch, std::vector<int> &outValue) const override;
		virtual void setInPatch(DataFile &patch, std::vector<int> value) const override;

		// SynthParameterLiveEditCapability
		virtual std::vector<MidiMessage> setValueMessages(std::shared_ptr<DataFile> const patch, Synth const *synth) const override;

		// SynthMultiLayerParameterCapability 
		virtual void setTargetLayer(int layerNo) override;
		virtual int getTargetLayer() const override;
		virtual void setSourceLayer(int layerNo) override;
		virtual int getSourceLayer() const override;
	private:
		ParamType type_;
		int targetLayer_; // The Rev2 has no layers, A (=0) and B (=0). By default, we target 0 but can change this calling setTargetLayer()
		int sourceLayer_;
		int number_;
		int endNumber_;
		int min_;
		int max_;
		int sysex_;
		std::string name_;
		std::function<std::string(int)> lookupFunction_;
	};

}
