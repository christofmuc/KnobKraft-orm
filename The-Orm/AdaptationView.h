/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "JuceHeader.h"

#include "GenericAdaptation.h"
#include "InfoText.h"

namespace knobkraft {

	class AdaptationView : public Component {
	public:
		AdaptationView();

		virtual void setupForAdaptation(std::shared_ptr<GenericAdaptation> adaptationSynth);

		virtual void resized() override;

	private:
		std::shared_ptr<GenericAdaptation> adaptation_;

		InfoText adaptationInfo_;
	};

}

