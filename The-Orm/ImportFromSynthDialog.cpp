/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "ImportFromSynthDialog.h"


ImportFromSynthDialog::ImportFromSynthDialog(midikraft::Synth *synth, TSuccessHandler onOk) : onOk_(onOk)
{
	addAndMakeVisible(propertyPanel_);
	addAndMakeVisible(cancel_);
	addAndMakeVisible(ok_);
	addAndMakeVisible(all_);
	ok_.setButtonText("Import selected");
	ok_.addListener(this);
	all_.setButtonText("Import all");
	all_.addListener(this);
	cancel_.setButtonText("Cancel");
	cancel_.addListener(this);

	// Populate the bank selector
	numBanks_ = synth->numberOfBanks();
	StringArray choices;
	Array<var> choiceValues;
	for (int i = 0; i < numBanks_; i++) {
		choices.add(synth->friendlyBankName(MidiBankNumber::fromZeroBase(i)));
		choiceValues.add(i);
	}
	bankValue_ = Array<var>();
	banks_ = new MultiChoicePropertyComponent(bankValue_, "Banks", choices, choiceValues);
	banks_->setExpanded(true);
	propertyPanel_.addProperties({ banks_});

	setBounds(0, 0, 400, 400);
}

void ImportFromSynthDialog::resized()
{
	Rectangle<int> area(getLocalBounds());
	auto bottom = area.removeFromBottom(40).reduced(8);
	int width = bottom.getWidth() / 3;
	ok_.setBounds(bottom.removeFromLeft(width).withTrimmedRight(8));
	all_.setBounds(bottom.removeFromLeft(width).withTrimmedRight(8));
	cancel_.setBounds(bottom);
	propertyPanel_.setBounds(area.reduced(8));
}

void ImportFromSynthDialog::buttonClicked(Button *button)
{
	if (button == &ok_) {
		// Close Window
		if (DialogWindow* dw = findParentComponentOfClass<DialogWindow>()) {
			dw->exitModalState(1);
		}
		std::vector<MidiBankNumber> result;
		var selected = bankValue_.getValue();
		for (auto bank : *selected.getArray()) {
			if ((int)bank < numBanks_) {
				result.push_back(MidiBankNumber::fromZeroBase((int)bank));
			}
			else {
				// All selected, just add all banks into the array
				jassertfalse;
			}
		}

		onOk_(result);
	}
	else if (button == &all_) {
		if (DialogWindow* dw = findParentComponentOfClass<DialogWindow>()) {
			dw->exitModalState(1);
		}
		std::vector<MidiBankNumber> result;
		for (int i = 0; i < numBanks_; i++) result.push_back(MidiBankNumber::fromZeroBase(i));
		onOk_(result);
	}
	else if (button == &cancel_) {
		if (DialogWindow* dw = findParentComponentOfClass<DialogWindow>()) {
			dw->exitModalState(-1);
		}
	}
}

