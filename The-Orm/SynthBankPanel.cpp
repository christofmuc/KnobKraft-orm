/*
   Copyright (c) 2022 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "SynthBankPanel.h"

#include "LayoutConstants.h"
#include "UIModel.h"
#include "PatchView.h"

#include <spdlog/spdlog.h>

SynthBankPanel::SynthBankPanel(midikraft::PatchDatabase& patchDatabase, PatchView *patchView)
	: patchDatabase_(patchDatabase), patchView_(patchView)
{
	instructions_.setText("This window displays a synth bank and acts as drop target to arrange patches. To start, select a synth bank via the Library tree.", juce::dontSendNotification);
	addAndMakeVisible(instructions_);

	resyncButton_.setButtonText("Import again");
	resyncButton_.onClick = [this]() {
		if (patchView_ && synthBank_) {
			if (!synthBank_->isDirty() ||
				AlertWindow::showOkCancelBox(AlertWindow::AlertIconType::QuestionIcon, "You have unsaved changes!",
					"You have modified the synth bank but not saved it back to the synth. Reimporting the bank will make you lose your changes! Do you want to re-import the bank from the synth?",
					"Yes", "Cancel")) {
				patchView_->retrieveBankFromSynth(synthBank_->synth(), synthBank_->bankNumber(), [this]() {
					temporaryBanks_.erase(synthBank_->id());
					auto toReload = synthBank_;
					synthBank_ = nullptr;
					patchView_->loadSynthBankFromDatabase(toReload->synth(), toReload->bankNumber(), toReload->id());
					});
			}	
		}
	};

	saveButton_.setButtonText("Save to database");
	saveButton_.onClick = [this]() {
		patchDatabase_.putPatchList(synthBank_);
		synthBank_->clearDirty();
		refresh();
	};

	sendButton_.setButtonText("Send to synth");
	sendButton_.onClick = [this]() {
		if (patchView_ && synthBank_) {
			if (isUserBank()) {
				patchView_->sendBankToSynth(synthBank_, true,  []() {
					spdlog::info("Bank sent successfully!");
				});
			} 
			else
			{
				patchView_->sendBankToSynth(synthBank_, false, [this]() {
					// Save it in the database now that we have successfully sent it to the synth
					patchDatabase_.putPatchList(synthBank_);
					// Mark the bank as not modified
					synthBank_->clearDirty();
					refresh();
				});
			}
		}
	};

	bankList_ = std::make_unique<VerticalPatchButtonList>([this](MidiProgramNumber programPlace, std::string md5) {
		if (synthBank_) {
			std::vector<midikraft::PatchHolder> result;
			if (patchDatabase_.getSinglePatch(synthBank_->synth(), md5, result)) {
				synthBank_->changePatchAtPosition(programPlace, result[0]);
				refresh();
			}
			else {
				spdlog::error("Program error - dropped patch that cannot be found in the database");
			}
		}
		}
		, [this](MidiProgramNumber program, std::string const& list_id, std::string const& list_name) {
			if (patchView_) {
				auto list = patchView_->retrieveListFromDatabase({ list_id, list_name });
				if (list) {
					// Insert the list into the bank...
					synthBank_->copyListToPosition(program, *list);
					refresh();
				}
			}
		}
		, [this](std::string const& list_id, std::string const& list_name) {
			if (patchView_) {
				auto list = patchView_->retrieveListFromDatabase({ list_id, list_name });
				if (list) {
					// Count how many patches are for our synth in that list
					int count = 0;
					for (auto const& patch : list->patches()) {
						if (patch.synth()->getName() == synthBank_->synth()->getName()) {
							count++;
						}
					}
					return count;
				}
			}
			return 1;
		});
	bankList_->onPatchClicked = [this](midikraft::PatchHolder& patch) {
		patchView_->selectPatch(patch, true);
	};
	addAndMakeVisible(synthName_);
	addAndMakeVisible(bankNameAndDate_);
	addAndMakeVisible(resyncButton_);
	addAndMakeVisible(saveButton_);
	addAndMakeVisible(sendButton_);
	addAndMakeVisible(modified_);
	addAndMakeVisible(*bankList_);

	showInfoIfRequired();

	// We need to know if our synth is turned off
	UIModel::instance()->synthList_.addChangeListener(this);
}

SynthBankPanel::~SynthBankPanel()
{
	UIModel::instance()->synthList_.removeChangeListener(this);
}

void SynthBankPanel::setBank(std::shared_ptr<midikraft::SynthBank> synthBank, PatchButtonInfo info)
{
	buttonMode_ = info;

	// If we have a synth bank already, move it aside to not lose potential changes to it
	if (synthBank_) {
		temporaryBanks_[synthBank_->id()] = synthBank_;
	}

	// Now take either the new bank, in case it is dirty or it is not found in the temp storage
	if (synthBank->isDirty() || temporaryBanks_.find(synthBank->id()) == temporaryBanks_.end()) {
		synthBank_ = synthBank;
	}
	else {
		// this bank is already known, so take the one from temp storage
		synthBank_ = temporaryBanks_[synthBank->id()];
	}
	refresh();
}

void SynthBankPanel::refresh() {
	if (synthBank_) {
		synthName_.setText(synthBank_->synth()->getName(), dontSendNotification);
		if (auto activeBank = std::dynamic_pointer_cast<midikraft::ActiveSynthBank>(synthBank_))
		{
			auto timeAgo = (juce::Time::getCurrentTime() - activeBank->lastSynced()).getApproximateDescription();
			std::string romText;
			if (!synthBank_->isWritable()) {
				romText = " [ROM]";
			}
			bankNameAndDate_.setText(fmt::format("Bank '{}'{} ({} ago)"
				, midikraft::SynthBank::friendlyBankName(synthBank_->synth(), synthBank_->bankNumber())
				, romText
				, timeAgo.toStdString())
				, dontSendNotification);
		}
		else
		{
			bankNameAndDate_.setText(fmt::format("Bank '{}' loading into '{}'", synthBank_->name(), synthBank_->targetBankName()), dontSendNotification);
		}
		bankList_->setPatches(synthBank_, buttonMode_);
		modified_.setText(synthBank_->isDirty() ? "modified" : "", dontSendNotification);
	}
	else
	{
		bankList_->clearList();
	}
	showInfoIfRequired();
}

void SynthBankPanel::resized()
{
	auto bounds = getLocalBounds();

	auto header = bounds.removeFromTop(LAYOUT_LARGE_LINE_SPACING * 2).reduced(LAYOUT_INSET_NORMAL);

	instructions_.setBounds(header);

	auto headerRightSide = header.removeFromRight(LAYOUT_BUTTON_WIDTH);

	auto upperButton = headerRightSide.removeFromTop(LAYOUT_BUTTON_HEIGHT);
	resyncButton_.setBounds(upperButton);
	saveButton_.setBounds(upperButton);
	sendButton_.setBounds(headerRightSide.removeFromTop(LAYOUT_BUTTON_HEIGHT + LAYOUT_INSET_NORMAL).withTrimmedTop(LAYOUT_INSET_NORMAL));
	synthName_.setBounds(header.removeFromTop(LAYOUT_LARGE_LINE_HEIGHT));
	bankNameAndDate_.setBounds(header.removeFromTop(LAYOUT_TEXT_LINE_HEIGHT));
	modified_.setBounds(header.removeFromTop(LAYOUT_TEXT_LINE_HEIGHT));

	bankList_->setBounds(bounds.reduced(LAYOUT_INSET_NORMAL));
}

void SynthBankPanel::changeListenerCallback(ChangeBroadcaster* source)
{
	if (source == &UIModel::instance()->synthList_)
	{
		if (synthBank_) {
			// Check if our synth is still active, if it is a DiscoverableDevice
			auto device = std::dynamic_pointer_cast<midikraft::SimpleDiscoverableDevice>(synthBank_->synth());
			jassert(device);
			if (!UIModel::instance()->synthList_.isSynthActive(device)) {
				synthBank_.reset();
				refresh();
			}
		}
	}
}

bool SynthBankPanel::isUserBank() {
	return std::dynamic_pointer_cast<midikraft::ActiveSynthBank>(synthBank_) == nullptr;
}

void SynthBankPanel::showInfoIfRequired() {
	bool showBank = synthBank_ != nullptr;
	instructions_.setVisible(!showBank);
	synthName_.setVisible(showBank);
	bankNameAndDate_.setVisible(showBank);
	modified_.setVisible(showBank);
	bool isUser = isUserBank();
	resyncButton_.setVisible(showBank && !isUser);
	saveButton_.setVisible(showBank && isUser && synthBank_->isDirty());
	sendButton_.setVisible(showBank && synthBank_->isWritable());
}