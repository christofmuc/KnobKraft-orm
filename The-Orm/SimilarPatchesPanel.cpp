#include "SimilarPatchesPanel.h"

#include "LayoutConstants.h"
#include "UIModel.h"
#include "PatchView.h"

#include <spdlog/spdlog.h>

/*Merkmal* SimilarityIndex::patchToFeatureVector(int key, size_t dimension, midikraft::PatchHolder const& patch)
{
	size_t num_dimensions = patch.patch()->data().size();
	auto merkmal = new Merkmal(key, (int)dimension);
	size_t i;
	for (i = 0; i < num_dimensions; i++) {
		merkmal->werte[i] = patch.patch()->data()[i] / 255.0f;
	}
	while (i < dimension) {
		merkmal->werte[i] = 0.0f;
		i++;
	}
	return merkmal;
}*/


SimilarPatchesPanel::SimilarPatchesPanel(PatchView* patchView, midikraft::PatchDatabase& db) : patchView_(patchView), db_(db)
	, buttonMode_(static_cast<PatchButtonInfo>(static_cast<int>(PatchButtonInfo::SubtitleSynth) | static_cast<int>(PatchButtonInfo::CenterName)))	
{
	similarity_ = std::make_unique<VerticalPatchButtonList>([](MidiProgramNumber, std::string) {},
		[](MidiProgramNumber, std::string const&, std::string const&) {},
		[this](std::string const&, std::string const&) { return (int) similarList_->patches().size(); });
	addAndMakeVisible(*similarity_);
	UIModel::instance()->currentPatch_.addChangeListener(this);
	similarList_ = std::make_shared<midikraft::PatchList>("SimilarPatches");
	similarity_->onPatchClicked = [this](midikraft::PatchHolder& patch) {
		patchView_->selectPatch(patch, true);
	};
	activeIndex_ = std::make_unique<PatchSimilarity>(db);
}

SimilarPatchesPanel::~SimilarPatchesPanel()
{
	UIModel::instance()->currentPatch_.removeChangeListener(this);
}

void SimilarPatchesPanel::resized()
{
	auto area = getLocalBounds();
	similarity_->setBounds(area.reduced(LAYOUT_INSET_NORMAL));
}

void SimilarPatchesPanel::changeListenerCallback(ChangeBroadcaster* source)
{
	if (source == &UIModel::instance()->currentPatch_)
	{
		if (UIModel::currentPatch().patch()) {
			// Search
			auto hits = activeIndex_->findSimilarPatches(UIModel::currentPatch(), 16);
			similarList_->setPatches(hits);
			similarity_->setPatchList(similarList_, buttonMode_);
		}
	}
}