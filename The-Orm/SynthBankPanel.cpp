/*
   Copyright (c) 2022 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "SynthBankPanel.h"

#include "LayoutConstants.h"
#include "UIModel.h"

#include "fmt/format.h"

SynthBankPanel::SynthBankPanel(midikraft::PatchDatabase& patchDatabase)
	: patchDatabase_(patchDatabase)
{
	resyncButton_.setButtonText("Import again");

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
	});
	addAndMakeVisible(synthName_);
	addAndMakeVisible(bankNameAndDate_);
	addAndMakeVisible(resyncButton_);
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

	resyncButton_.setBounds(header.removeFromRight(LAYOUT_BUTTON_WIDTH).withHeight(LAYOUT_BUTTON_HEIGHT));
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
