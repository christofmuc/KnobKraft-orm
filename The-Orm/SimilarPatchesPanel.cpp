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

Merkmal * SimilarPatchesPanel::patchToFeatureVector(int key, size_t dimension, midikraft::PatchHolder const& patch)
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
}

void SimilarPatchesPanel::changeListenerCallback(ChangeBroadcaster* source)
{
	if (source == &UIModel::instance()->currentPatch_)
	{
		if (UIModel::currentPatch().patch()) {
			// Build the VPtree
			EuklidMass cityBlock;

			// Load the patches
			auto allPatches = db_.getPatches(patchView_->currentFilter(), 0, -1);
			auto max_element = std::max_element(allPatches.cbegin(), allPatches.cend(), [](midikraft::PatchHolder const& a, midikraft::PatchHolder const& b) { 
				return (a.patch() ? a.patch()->data().size() : 0) < (b.patch() ? b.patch()->data().size() : 0);
			});
			size_t max_dimension = 0;
			if (max_element != allPatches.cend()) {
				max_dimension = max_element->patch()->data().size();
			}
			auto features = new Merkmal * [allPatches.size()];
			for (size_t i = 0; i < allPatches.size(); i++) {
				features[i] = patchToFeatureVector((int)i, max_dimension, allPatches[i]);
			}
			auto featureSet = new MerkmalsMenge((int)allPatches.size(), features);

			VPBaum vp_tree("test.vp", &cityBlock, (int)max_dimension, 16, 2);
			long start = vp_tree.speichereMenge(featureSet);
			vp_tree.info.startSeite = start;
			vp_tree.speichereInfo();
			for (size_t i = 0; i < allPatches.size(); i++) {
				delete features[i];
			}
			delete featureSet;

			// Run the VPsearch here
			float sigma = 10000.0f;
			Merkmal* referencePatch = patchToFeatureVector(-1, max_dimension,  UIModel::currentPatch());
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