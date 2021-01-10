/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "JuceHeader.h"

#include "Synth.h"
#include "MidiBankNumber.h"
#include "ProgressHandler.h"

class ImportFromSynthDialog : public Component, private Button::Listener
{
public:
	struct SelectedDataTypes{
		bool isDataImport;
		int dataTypeID;
		int startIndex;
		MidiBankNumber bank = MidiBankNumber::invalid();
	};
	typedef std::function<void(std::vector<SelectedDataTypes> bankNo)> TSuccessHandler;

	ImportFromSynthDialog(midikraft::Synth *synth, TSuccessHandler onOk);
	virtual ~ImportFromSynthDialog() = default;

	void resized() override;
	void buttonClicked(Button*) override;

private:
	midikraft::Synth *synth_;
	TSuccessHandler onOk_;
	MultiChoicePropertyComponent  *banks_;
	PropertyPanel propertyPanel_;
	TextButton ok_, all_, cancel_;

	Value bankValue_;
	int numBanks_; 

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ImportFromSynthDialog)
};

