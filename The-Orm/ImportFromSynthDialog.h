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
	typedef std::function<void(std::vector<MidiBankNumber> bankNo)> TSuccessHandler;

	ImportFromSynthDialog(midikraft::Synth *synth, TSuccessHandler onOk);
	virtual ~ImportFromSynthDialog() = default;

	void resized() override;
	void buttonClicked(Button*) override;

private:
	TSuccessHandler onOk_;
	MultiChoicePropertyComponent  *banks_;
	PropertyPanel propertyPanel_;
	TextButton ok_, all_, cancel_;

	Value bankValue_;
	int numBanks_; 

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ImportFromSynthDialog)
};

//TODO - this has turned into a pure progress handler, and has nothing to do with the importfromsynth anymore. I should put this into juce-widgets?
class ImportFromSynthThread : public ThreadWithProgressWindow, public midikraft::ProgressHandler
{
public:
	ImportFromSynthThread();

	void run() override;

	bool shouldAbort() const override;
	void setProgressPercentage(double zeroToOne) override;
	void onSuccess() override;
	void onCancel() override;
};
