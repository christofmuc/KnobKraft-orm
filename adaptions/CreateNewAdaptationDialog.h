/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "JuceHeader.h"

namespace knobkraft {

	class CreateNewAdaptationDialog : public Component, private TextButton::Listener {
	public:
		CreateNewAdaptationDialog();

		virtual void resized() override;

		static void showDialog(Component *center);

	private:
		void buttonClicked(Button*) override;
		bool createNewAdaptation();

		ComboBox template_;
		Label text_;
		Label basedOn_;
		TextButton ok_, cancel_;

		static std::unique_ptr<CreateNewAdaptationDialog> dialog_;

		JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CreateNewAdaptationDialog)
	};

}
