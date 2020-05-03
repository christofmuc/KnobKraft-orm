/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "ImportFromSynthDialog.h"

class ImportFromSynthThread : public ThreadWithProgressWindow, public midikraft::ProgressHandler
{
public:
	ImportFromSynthThread(ImportFromSynthDialog::TBankLoadHandler onOk) : ThreadWithProgressWindow("Importing...", true, true), onOk_(onOk), bank_(MidiBankNumber::fromZeroBase(0))
	{
	}

	void run() override
	{
		stop_ = false;
		onOk_(bank_, this);
		while (!stop_ && !threadShouldExit()) {
			Thread::sleep(100);
		}
	}

	void setBank(MidiBankNumber id) {
		bank_ = id;
	}

	bool shouldAbort() const override
	{
		return threadShouldExit();
	}

	void setProgressPercentage(double zeroToOne) override
	{
		setProgress(zeroToOne);
	}

	void onSuccess() override
	{
		stop_ = true;
	}

	void onCancel() override
	{
		stop_ = true;
	}

private:
	ImportFromSynthDialog::TBankLoadHandler onOk_;
	bool stop_;
	MidiBankNumber bank_;
};

ImportFromSynthDialog::ImportFromSynthDialog(midikraft::Synth *synth, TBankLoadHandler onOk) : onOk_(onOk)
{
	thread_ = std::make_unique<ImportFromSynthThread>(onOk);
	setBounds(0, 0, 400, 100);
	addAndMakeVisible(&bank_);
	addAndMakeVisible(&cancel_);
	addAndMakeVisible(&ok_);
	ok_.setButtonText("OK");
	ok_.addListener(this);
	cancel_.setButtonText("Cancel");
	cancel_.addListener(this);

	// Populate the bank selector
	int numBanks = synth->numberOfBanks();
	for (int i = 0; i < numBanks; i++) {
		bank_.addItem(synth->friendlyBankName(MidiBankNumber::fromZeroBase(i)), i + 1);
	}
	bank_.setSelectedItemIndex(0, dontSendNotification);
}

void ImportFromSynthDialog::resized()
{
	Rectangle<int> area(getLocalBounds());
	auto bottom = area.removeFromBottom(40).reduced(8);
	ok_.setBounds(bottom.removeFromLeft(bottom.getWidth() / 2));
	cancel_.setBounds(bottom);
	bank_.setBounds(area.reduced(8));
}

void ImportFromSynthDialog::buttonClicked(Button *button)
{
	if (button == &ok_) {
		// Close Window
		if (DialogWindow* dw = findParentComponentOfClass<DialogWindow>()) {
			dw->exitModalState(1);
		}
		thread_->setBank(MidiBankNumber::fromOneBase(bank_.getSelectedId()));
		thread_->runThread();
	}
	else if (button == &cancel_) {
		if (DialogWindow* dw = findParentComponentOfClass<DialogWindow>()) {
			dw->exitModalState(-1);
		}
	}
}

