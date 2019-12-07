#pragma once

#include "SynthParameterDefinition.h"
#include "NrpnDefinition.h"

namespace midikraft {

	class Rev2ParamDefinition : public SynthParameterDefinition, 
		public SynthVectorParameterCapability, public SynthLookupParameterCapability, public SynthParameterLiveEditCapability {
	public:
		Rev2ParamDefinition(NrpnDefinition const &nrpn);

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
		NrpnDefinition nrpn_;
	};

}
