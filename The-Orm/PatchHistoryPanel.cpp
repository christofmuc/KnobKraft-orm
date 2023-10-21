#include "PatchHistoryPanel.h"

#include "LayoutConstants.h"
#include "UIModel.h"

PatchHistoryPanel::PatchHistoryPanel(PatchView* patchView) : patchView_(patchView)
	, buttonMode_(static_cast<PatchButtonInfo>(static_cast<int>(PatchButtonInfo::SubtitleSynth) | static_cast<int>(PatchButtonInfo::CenterName)))	
{
	history_ = std::make_unique<VerticalPatchButtonList>([](MidiProgramNumber, std::string) {}, 
		[](MidiProgramNumber, std::string const&, std::string const&) {},
		[this](std::string const&, std::string const&) { return (int) patchHistory_->patches().size(); });
	addAndMakeVisible(*history_);
	UIModel::instance()->currentPatch_.addChangeListener(this);
	patchHistory_ = std::make_shared<midikraft::PatchList>("History");

}

PatchHistoryPanel::~PatchHistoryPanel()
{
	UIModel::instance()->currentPatch_.removeChangeListener(this);
}

void PatchHistoryPanel::resized()
{
	auto area = getLocalBounds();
	history_->setBounds(area.reduced(LAYOUT_INSET_NORMAL));
}

void PatchHistoryPanel::changeListenerCallback(ChangeBroadcaster* source)
{
	if (source == &UIModel::instance()->currentPatch_)
	{
		patchHistory_->insertPatchAtTopAndRemoveDuplicates(UIModel::currentPatch());
		history_->setPatchList(patchHistory_, buttonMode_);
	}
}