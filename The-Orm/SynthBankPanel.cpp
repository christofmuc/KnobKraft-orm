#include "SynthBankPanel.h"

#include "LayoutConstants.h"

SynthBankPanel::SynthBankPanel()
{
	bankList_ = std::make_unique<VerticalPatchButtonList>(true);
	addAndMakeVisible(*bankList_);
}

void SynthBankPanel::setPatches(std::vector<midikraft::PatchHolder> const& patches, PatchButtonInfo info)
{
	bankList_->setPatches(patches, info);
}

void SynthBankPanel::resized()
{
	auto bounds = getLocalBounds();

	auto header = bounds.removeFromTop(LAYOUT_LARGE_LINE_SPACING);

	bankList_->setBounds(bounds);
}
