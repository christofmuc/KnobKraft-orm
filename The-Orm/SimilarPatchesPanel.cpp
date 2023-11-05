#include "SimilarPatchesPanel.h"

#include "LayoutConstants.h"
#include "UIModel.h"
#include "PatchView.h"

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

Merkmal * SimilarPatchesPanel::patchToFeatureVector(int key, midikraft::PatchHolder const& patch)
{
	size_t num_dimensions = patch.patch()->data().size();
	auto values = new float[num_dimensions];
	for (size_t i = 0; i < num_dimensions; i++) {
		values[i] = patch.patch()->data()[i] / 255.0f;
	}
	return new Merkmal(key, (int) num_dimensions, values);
}

void SimilarPatchesPanel::changeListenerCallback(ChangeBroadcaster* source)
{
	if (source == &UIModel::instance()->currentPatch_)
	{
		if (UIModel::currentPatch().patch()) {
			// Build the VPtree
			EuklidMass cityBlock;
			size_t dimension = UIModel::currentPatch().patch()->data().size();
			VPBaum vp_tree("test.vp", &cityBlock, (int)dimension, 16, 2);

			// Load the patches
			auto allPatches = db_.getPatches(patchView_->currentFilter(), 0, -1);
			auto features = new Merkmal * [allPatches.size()];
			for (size_t i = 0; i < allPatches.size(); i++) {
				features[i] = patchToFeatureVector((int)i, allPatches[i]);
			}
			auto featureSet = new MerkmalsMenge((int)allPatches.size(), features);
			long start = vp_tree.speichereMenge(featureSet);
			vp_tree.info.startSeite = start;
			vp_tree.speichereInfo();
			//for (size_t i = 0; i < allPatches.size(); i++) {
//					delete features[i];
			//}
			delete featureSet;

			// Run the VPsearch here
			float sigma = 0.001f;
			Merkmal* referencePatch = patchToFeatureVector(-1, UIModel::currentPatch());
			VPBaum vp_tree_search("test.vp");
			KArray* result = vp_tree_search.kNNSuche(referencePatch, 10, &sigma);
			assert(result != nullptr);

			// Build a list from the results!
			std::vector<midikraft::PatchHolder> hits;
			for (int i = 0; i < result->getNum(); i++) {
				hits.push_back(allPatches[result->getItem(i)->schluessel]);
			}
			similarList_->setPatches(hits);
			similarity_->setPatchList(similarList_, buttonMode_);

			delete referencePatch;
		}
	}
}