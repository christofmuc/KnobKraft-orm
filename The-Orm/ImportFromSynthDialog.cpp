/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "ImportFromSynthDialog.h"

#include "Capability.h"
#include "DataFileLoadCapability.h"

ImportFromSynthDialog::ImportFromSynthDialog(midikraft::Synth *synth, TSuccessHandler onOk) : synth_(synth), onOk_(onOk)
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

	StringArray choices;
	Array<var> choiceValues;
	auto dfl = midikraft::Capability::hasCapability<midikraft::DataFileLoadCapability>(synth);
	if (dfl) {
		// This is for synths that support multiple data types. They will want to generate their bank list on their own
		auto imports = dfl->dataFileImportChoices();
		for (int i = 0; i < imports.size(); i++) {
			choices.add(imports[i].description);
			choiceValues.add(i);
		}
		bankValue_ = Array<var>();
		banks_ = new MultiChoicePropertyComponent(bankValue_, "Data", choices, choiceValues);
	}
	else {
		// Simple - we only have banks, so just populate the bank selector
		numBanks_ = synth->numberOfBanks();
		for (int i = 0; i < numBanks_; i++) {
			choices.add(synth->friendlyBankName(MidiBankNumber::fromZeroBase(i)));
			choiceValues.add(i);
		}
		bankValue_ = Array<var>();
		banks_ = new MultiChoicePropertyComponent(bankValue_, "Banks", choices, choiceValues);
	}
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
	auto dfl = midikraft::Capability::hasCapability<midikraft::DataFileLoadCapability>(synth_);
	std::vector<SelectedImports> result;
	if (button == &ok_) {
		// Close Window
		if (DialogWindow* dw = findParentComponentOfClass<DialogWindow>()) {
			dw->exitModalState(1);
		}

		if (dfl) {
			var selected = bankValue_.getValue();
			auto imports = dfl->dataFileImportChoices();
			for (auto index : *selected.getArray()) {
				result.push_back({ true, imports[(int)index] });
			}
		}
		else {
			var selected = bankValue_.getValue();
			for (auto bank : *selected.getArray()) {
				SelectedImports checked;
				checked.isDataImport = false;
				checked.bank = MidiBankNumber::fromZeroBase((int)bank);
				result.push_back(checked);
			}
		}
		onOk_(result);
	}
	else if (button == &all_) {
		if (DialogWindow* dw = findParentComponentOfClass<DialogWindow>()) {
			dw->exitModalState(1);
		}
		if (dfl) {
			auto data_types = dfl->dataTypeNames();
			auto imports = dfl->dataFileImportChoices();
			for (int i = 0; i < imports.size(); i++) {
				result.push_back({ true, imports[i]});
			}
		}
		else {
			for (int i = 0; i < numBanks_; i++) {
				SelectedImports checked;
				checked.isDataImport = false;
				checked.bank = MidiBankNumber::fromZeroBase(i);
				result.push_back(checked);
			}
		}
		onOk_(result);
	}
	else if (button == &cancel_) {
		if (DialogWindow* dw = findParentComponentOfClass<DialogWindow>()) {
			dw->exitModalState(-1);
		}
	}
}

