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

	exportButton_.setButtonText("Export bank");
	exportButton_.onClick = [this]() {
		patchView_->exportBank();
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
	addAndMakeVisible(exportButton_);
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
	if (synthBank_ && synthBank_->id() != synthBank->id()) {
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

void SynthBankPanel::refreshPatch(std::shared_ptr<midikraft::PatchHolder> updatedPatch)
{
	if (synthBank_) {
		bool refreshRequired = false;
		for (auto patch : synthBank_->patches()) {
			if (patch.md5() == updatedPatch->md5() && patch.smartSynth()->getName() == updatedPatch->smartSynth()->getName()) {
				synthBank_->updatePatchAtPosition(patch.patchNumber(), *updatedPatch);
				refreshRequired = true;
			}
		}
		if (refreshRequired)
		{
			refresh();
		}
	}
}

void SynthBankPanel::reloadFromDatabase()
{
	if (synthBank_)
	{
		std::map<std::string, std::weak_ptr<midikraft::Synth>> synthMap;
		auto listInfo = midikraft::ListInfo({ synthBank_->id(), synthBank_->name()});
		synthMap.emplace(synthBank_->synth()->getName(), synthBank_->synth());
		// This is not too bad, but actually if the current bank is dirty I should not just lose everything just because a patch was renamed.
		// I need to reload the patches from the database, but don't modify the list itself (and not it's dirty step!)
		auto newList = patchDatabase_.getPatchList(listInfo, synthMap);
		if (auto synthBank = std::dynamic_pointer_cast<midikraft::SynthBank>(newList)) {
			jassert(synthBank->isDirty());
			setBank(synthBank, buttonMode_);
			refresh();
		}
		else {
			spdlog::error("Program error - bank refreshed for SynthBankPanel is no longer a Synthbank");
		}
	}
}

std::shared_ptr<midikraft::SynthBank> SynthBankPanel::getCurrentSynthBank() const
{
	return synthBank_;
}

void SynthBankPanel::copyPatchNamesToClipboard()
{
    if (synthBank_) {
        StringArray allNames;
        for (auto const& patch : synthBank_->patches()) {
            allNames.add(patch.name());
        }
        SystemClipboard::copyTextToClipboard(allNames.joinIntoString("\n"));
    }
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
			bankNameAndDate_.setText(fmt::format("'{}'{} ({} ago)"
				, midikraft::SynthBank::friendlyBankName(synthBank_->synth(), synthBank_->bankNumber())
				, romText
				, timeAgo.toStdString())
				, dontSendNotification);
		}
		else
		{
			bankNameAndDate_.setText(fmt::format("'{}' loading into '{}'", synthBank_->name(), synthBank_->targetBankName()), dontSendNotification);
		}
		bankList_->setPatchList(synthBank_, buttonMode_);
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

	auto header = bounds.removeFromTop(LAYOUT_LARGE_LINE_SPACING * 3).reduced(LAYOUT_INSET_NORMAL);

	instructions_.setBounds(header);

	auto headerRightSide = header.removeFromRight(LAYOUT_BUTTON_WIDTH);

	auto upperButton = headerRightSide.removeFromTop(LAYOUT_BUTTON_HEIGHT);
	resyncButton_.setBounds(upperButton);
	saveButton_.setBounds(upperButton);
	sendButton_.setBounds(headerRightSide.removeFromTop(LAYOUT_BUTTON_HEIGHT + LAYOUT_INSET_NORMAL).withTrimmedTop(LAYOUT_INSET_NORMAL));
	exportButton_.setBounds(headerRightSide.removeFromTop(LAYOUT_BUTTON_HEIGHT + LAYOUT_INSET_NORMAL).withTrimmedTop(LAYOUT_INSET_NORMAL));
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
	exportButton_.setVisible(showBank );
}