#pragma once

#include "Patch.h"

#include "NrpnDefinition.h"

#include "LayeredPatch.h"

namespace midikraft {

	class Rev2PatchNumber : public PatchNumber {
	public:
		using PatchNumber::PatchNumber;
		virtual std::string friendlyName() const override;

	private:
		int bank_; // 0 to 7 for banks U1 to U4 and F1 to F4
	};

	class Rev2Patch : public Patch, public LayeredPatch {
	public:
		Rev2Patch();
		Rev2Patch(Synth::PatchData const &patchData);

		virtual std::string patchName() const override;
		virtual void setName(std::string const &name) override;
		virtual std::shared_ptr<PatchNumber> patchNumber() const override;
		virtual void setPatchNumber(MidiProgramNumber patchNumber) override;
		virtual int value(SynthParameterDefinition const &param) const override;
		virtual SynthParameterDefinition const & paramBySysexIndex(int sysexIndex) const override;
		virtual std::vector<std::string> warnings() override;

		void addNrpns(std::vector<NRPNValue> const &values);
		//int addOsc2Patch(SimpleOsc const &osc);
		//void addEnv2Patch(Envelope const &env, int targetEnvNo);

		virtual std::vector<SynthParameterDefinition *> allParameterDefinitions() override;

		// Interface for layered Patches (for the Librarian)
		virtual LayerMode layerMode() const override;
		virtual int numberOfLayers() const override;
		virtual std::string layerName(int layerNo) const override;

	private:
		Rev2PatchNumber number_;
		std::vector<std::string> warnings_;
		int oscAllocated_;
	};

}

