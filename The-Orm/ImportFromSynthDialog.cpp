/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "ImportFromSynthDialog.h"


ImportFromSynthDialog::ImportFromSynthDialog(std::shared_ptr<midikraft::Synth> synth, TSuccessHandler onOk) : onOk_(onOk)
{
	addAndMakeVisible(propertyPanel_);
	addAndMakeVisible(cancel_);
	addAndMakeVisible(ok_);
	addAndMakeVisible(all_);
	ok_.setButtonText("Import selected");
	ok_.onClick = [this, synth]() {
		// Close Window
		if (DialogWindow* dw = findParentComponentOfClass<DialogWindow>()) {
			dw->exitModalState(1);
		}
		std::vector<MidiBankNumber> result;
		var selected = bankValue_.getValue();
		for (auto bank : *selected.getArray()) {
			if ((int)bank < numBanks_) {
				result.push_back(MidiBankNumber::fromZeroBase((int)bank, synth->numberOfPatches()));
			}
			else {
				// All selected, just add all banks into the array
				jassertfalse;
			}
		}

		onOk_(result);
	};
	all_.setButtonText("Import all");
	all_.onClick = [this, synth]() {
		if (DialogWindow* dw = findParentComponentOfClass<DialogWindow>()) {
			dw->exitModalState(1);
		}
		std::vector<MidiBankNumber> result;
		for (int i = 0; i < numBanks_; i++) result.push_back(MidiBankNumber::fromZeroBase(i, synth->numberOfPatches()));
		onOk_(result);
	};
	cancel_.setButtonText("Cancel");
	cancel_.onClick = [this]() {
		if (DialogWindow* dw = findParentComponentOfClass<DialogWindow>()) {
			dw->exitModalState(-1);
		}
	};

	// Populate the bank selector
	numBanks_ = synth->numberOfBanks();
	StringArray choices;
	Array<var> choiceValues;
	for (int i = 0; i < numBanks_; i++) {
		choices.add(synth->friendlyBankName(MidiBankNumber::fromZeroBase(i, synth->numberOfPatches())));
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

