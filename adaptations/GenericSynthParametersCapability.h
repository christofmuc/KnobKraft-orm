/*
   Copyright (c) 2025 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "JuceHeader.h"

#include "DetailedParametersCapability.h"

namespace knobkraft {

	class GenericAdaptation;

	class GenericSynthParametersCapability : public midikraft::SynthParametersCapability
	{
	public:
		explicit GenericSynthParametersCapability(GenericAdaptation* me) : me_(me) {}
		virtual ~GenericSynthParametersCapability() = default;

		std::vector<midikraft::ParamDef> getParameterDefinitions() const override;
		std::vector<midikraft::ParamVal> getParameterValues(std::shared_ptr<midikraft::DataFile> const patch, bool onlyActive) const override;
		bool setParameterValues(std::shared_ptr<midikraft::DataFile> patch, std::vector<midikraft::ParamVal> const& new_values) override;
		std::vector<float> createFeatureVector(std::shared_ptr<midikraft::DataFile> const patch) const override;

	private:
		GenericAdaptation* me_;
	};

}
