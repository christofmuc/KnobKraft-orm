/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "JuceHeader.h"

#include "Synth.h"
#include "MidiBankNumber.h"
#include "ProgressHandler.h"

class ImportFromSynthThread;

class ImportFromSynthDialog : public Component, private Button::Listener
{
public:
	typedef std::function<void(midikraft::MidiBankNumber bankNo, midikraft::ProgressHandler *)> TBankLoadHandler;

	ImportFromSynthDialog(midikraft::Synth *synth, TBankLoadHandler onOk);

	void resized() override;
	void buttonClicked(Button*) override;

private:
	TBankLoadHandler onOk_;
	std::unique_ptr<ImportFromSynthThread> thread_;
	ComboBox bank_;
	TextButton ok_, cancel_;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ImportFromSynthDialog)
};
