/*
   Copyright (c) 2019 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "Patch.h"
#include "Rev2ParamDefinition.h"
#include "StoredPatchNameCapability.h"
#include "DetailedParametersCapability.h"

#include "LayeredPatchCapability.h"

namespace midikraft {

	class Rev2Patch : public Patch, public LayeredPatchCapability, public StoredPatchNameCapability, public DefaultNameCapability, public DetailedParametersCapability {
	public:
		Rev2Patch();
		Rev2Patch(Synth::PatchData const &patchData, MidiProgramNumber programNo);

		virtual std::string name() const override;
		virtual void setName(std::string const &name) override;
		virtual bool isDefaultName(std::string const &patchName) const override;
		virtual MidiProgramNumber patchNumber() const override;

		virtual std::vector<std::shared_ptr<SynthParameterDefinition>> allParameterDefinitions() const override;

		// Interface for layered Patches (for the Librarian)
		virtual LayerMode layerMode() const override;
		virtual int numberOfLayers() const override;
		virtual std::string layerName(int layerNo) const override;
		virtual void setLayerName(int layerNo, std::string const &layerName) override;

		static std::shared_ptr<Rev2ParamDefinition> find(std::string const &paramID);

	private:
		MidiProgramNumber number_;
	};

}

