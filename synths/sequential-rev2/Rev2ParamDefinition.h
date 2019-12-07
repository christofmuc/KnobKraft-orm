#pragma once

#include "SynthParameterDefinition.h"
#include "NrpnDefinition.h"

namespace midikraft {

	class Rev2ParamDefinition : public SynthParameterDefinition {
	public:
		Rev2ParamDefinition(NrpnDefinition const &nrpn);
		virtual ParamType type() const override;

		virtual std::string name() const override;
		virtual std::string description() const override;

		virtual std::string valueAsText(int value) const override;

		virtual int sysexIndex() const override;
		virtual int endSysexIndex() const override;

		virtual bool matchesController(int controllerNumber) const override;
		virtual int minValue() const override;
		virtual int maxValue() const override;

		virtual bool isActive(Patch const *patch) const override;

		virtual bool valueInPatch(Patch const &patch, int &outValue) const override;
		virtual bool valueInPatch(Patch const &patch, std::vector<int> &outValue) const override;
		virtual std::string valueInPatchToText(Patch const &patch) const override;

		virtual void setInPatch(Patch &patch, int value) const override;
		virtual void setInPatch(Patch &patch, std::vector<int> value) const override;

		virtual MidiBuffer setValueMessage(Patch const &patch, Synth *synth) const override;

	private:
		NrpnDefinition nrpn_;
	};

}
