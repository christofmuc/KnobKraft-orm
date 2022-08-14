/*
   Copyright (c) 2022 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "SynthBankPanel.h"

#include "LayoutConstants.h"
#include "UIModel.h"
#include "PatchView.h"

#include "fmt/format.h"

SynthBankPanel::SynthBankPanel(midikraft::PatchDatabase& patchDatabase, PatchView *patchView)
	: patchDatabase_(patchDatabase), patchView_(patchView)
{
	resyncButton_.setButtonText("Import again");
	resyncButton_.onClick = [this]() {
		if (patchView_ && synthBank_) {
			if (!UIModel::instance()->synthBank.modified() ||
				AlertWindow::showOkCancelBox(AlertWindow::AlertIconType::QuestionIcon, "You have unsaved changes!",
					"You have modified the synth bank but not saved it back to the synth. Reimporting the bank will make you lose your changes! Do you want to re-import the bank from the synth?",
					"Yes", "Cancel")) {
				patchView_->retrieveBankFromSynth(*synthBank_, {});
			}
		}
	};

	sendButton_.setButtonText("Send to synth");
	sendButton_.onClick = [this]() {
		if (patchView_ && synthBank_) {
			patchView_->sendBankToSynth(synthBank_, [this]() {
				// Save it in the database now that we have successfully sent it to the synth
				patchDatabase_.putPatchList(synthBank_);
				// Mark the bank as not modified
				UIModel::instance()->synthBank.clearModified();
				if (bankList_) {
					bankList_->refreshContent();
				}
			});
		}
	};

	bankList_ = std::make_unique<VerticalPatchButtonList>([this](MidiProgramNumber programPlace, std::string md5) {
		if (synthBank_) {
			std::vector<midikraft::PatchHolder> result;
			if (patchDatabase_.getSinglePatch(synthBank_->synth(), md5, result)) {
				synthBank_->changePatchAtPosition(programPlace, result[0]);
				UIModel::instance()->synthBank.flagModified();
			}
			else {
				SimpleLogger::instance()->postMessage("Program error - dropped patch that cannot be found in the database");
			}
		}
		}
		, [this](MidiProgramNumber program, std::string const& list_id, std::string const& list_name) {
			if (patchView_) {
				auto list = patchView_->retrieveListFromDatabase({ list_id, list_name });
				if (list) {
					// Insert the list into the bank...
					synthBank_->copyListToPosition(program, *list);
					UIModel::instance()->synthBank.flagModified();
				}
			}
		}
		, [this](std::string const& list_id, std::string const& list_name) {
			if (patchView_) {
				auto list = patchView_->retrieveListFromDatabase({ list_id, list_name });
				if (list) {
					return (int) list->patches().size();
				}
			}
			return 1;
		});
	addAndMakeVisible(synthName_);
	addAndMakeVisible(bankNameAndDate_);
	addAndMakeVisible(resyncButton_);
	addAndMakeVisible(sendButton_);
	addAndMakeVisible(modified_);
	addAndMakeVisible(*bankList_);

	UIModel::instance()->synthBank.addChangeListener(this);
}

SynthBankPanel::~SynthBankPanel()
{
	UIModel::instance()->synthBank.removeChangeListener(this);
}

void SynthBankPanel::setBank(std::shared_ptr<midikraft::SynthBank> synthBank, PatchButtonInfo info)
{
	synthBank_ = synthBank;
	synthName_.setText(synthBank->synth()->getName(), dontSendNotification);
    auto timeAgo = (juce::Time::getCurrentTime() - synthBank_->lastSynced()).getApproximateDescription();
	bankNameAndDate_.setText(fmt::format("Bank {} ({} ago)", synthBank_->synth()->friendlyBankName(synthBank_->bankNumber()), timeAgo.toStdString()), dontSendNotification);
	bankList_->setPatches(synthBank, info);
}

void SynthBankPanel::resized()
{
	auto bounds = getLocalBounds();

	auto header = bounds.removeFromTop(LAYOUT_LARGE_LINE_SPACING * 2);

	auto headerRightSide = header.removeFromRight(LAYOUT_BUTTON_WIDTH);

	resyncButton_.setBounds(headerRightSide.removeFromTop(LAYOUT_BUTTON_HEIGHT));
	sendButton_.setBounds(headerRightSide.removeFromTop(LAYOUT_BUTTON_HEIGHT + LAYOUT_INSET_NORMAL).withTrimmedTop(LAYOUT_INSET_NORMAL));
	synthName_.setBounds(header.removeFromTop(LAYOUT_LARGE_LINE_HEIGHT));
	bankNameAndDate_.setBounds(header.removeFromTop(LAYOUT_TEXT_LINE_HEIGHT));
	modified_.setBounds(header.removeFromTop(LAYOUT_TEXT_LINE_HEIGHT));

	bankList_->setBounds(bounds);
}

void SynthBankPanel::changeListenerCallback(ChangeBroadcaster* source)
{
	ignoreUnused(source);
	setBank(UIModel::instance()->synthBank.synthBank(), PatchButtonInfo::DefaultDisplay);
	modified_.setText(UIModel::instance()->synthBank.modified() ? "modified" : "", dontSendNotification);
}
