/*
   Copyright (c) 2022 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "SynthBankPanel.h"

#include "LayoutConstants.h"

#include "fmt/format.h"

SynthBankPanel::SynthBankPanel() 
{
	resyncButton_.setButtonText("Import again");

	bankList_ = std::make_unique<VerticalPatchButtonList>(true);
	addAndMakeVisible(synthName_);
	addAndMakeVisible(bankNameAndDate_);
	addAndMakeVisible(resyncButton_);
	addAndMakeVisible(*bankList_);
}

void SynthBankPanel::setBank(std::shared_ptr<midikraft::Synth> synth, MidiBankNumber bank, juce::Time timestamp)
{
	synthName_.setText(synth->getName(), dontSendNotification);
    auto timeAgo = (juce::Time::getCurrentTime() - timestamp).getApproximateDescription();
	bankNameAndDate_.setText(fmt::format("Bank {} ({} ago)", synth->friendlyBankName(bank), timeAgo.toStdString()), dontSendNotification);
}

void SynthBankPanel::setPatches(std::vector<midikraft::PatchHolder> const& patches, PatchButtonInfo info)
{
	bankList_->setPatches(patches, info);
}

void SynthBankPanel::resized()
{
	auto bounds = getLocalBounds();

	auto header = bounds.removeFromTop(LAYOUT_LARGE_LINE_SPACING * 2);

	resyncButton_.setBounds(header.removeFromRight(LAYOUT_BUTTON_WIDTH).withHeight(LAYOUT_BUTTON_HEIGHT));
	synthName_.setBounds(header.removeFromTop(LAYOUT_LARGE_LINE_HEIGHT));
	bankNameAndDate_.setBounds(header.removeFromTop(LAYOUT_LARGE_LINE_HEIGHT));

	bankList_->setBounds(bounds);
}
