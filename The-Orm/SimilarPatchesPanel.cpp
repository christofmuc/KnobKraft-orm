#include "SimilarPatchesPanel.h"

#include "LayoutConstants.h"
#include "UIModel.h"
#include "PatchView.h"

#include <spdlog/spdlog.h>

SimilarPatchesPanel::SimilarPatchesPanel(PatchView* patchView, midikraft::PatchDatabase& db) : patchView_(patchView), db_(db)
	, buttonMode_(static_cast<PatchButtonInfo>(static_cast<int>(PatchButtonInfo::SubtitleSynth) | static_cast<int>(PatchButtonInfo::CenterName))),
	similarityValue_(Slider::LinearHorizontal, Slider::NoTextBox)
{
	similarity_ = std::make_unique<VerticalPatchButtonList>([](MidiProgramNumber, std::string) {},
		[](MidiProgramNumber, std::string const&, std::string const&) {},
		[this](std::string const&, std::string const&) { return (int) similarList_->patches().size(); });
	addAndMakeVisible(*similarity_);
	UIModel::instance()->currentPatch_.addChangeListener(this);
	UIModel::instance()->databaseChanged.addChangeListener(this);
	similarList_ = std::make_shared<midikraft::PatchList>("SimilarPatches");
	similarity_->onPatchClicked = [this](midikraft::PatchHolder& patch) {
		patchView_->selectPatch(patch, true);
	};
	activeIndex_ = std::make_unique<PatchSimilarity>(db);

	helpText_.setText("Experimental feature - click on a patch in the grid, and after creating an in-memory index this list will show patches which are similar.\n" 
		"Cutoff selects how similar hits must be to be shown. Two different metrics for testing.");
	helpText_.setEnabled(false);
	helpText_.setMultiLine(true);
	addAndMakeVisible(helpText_);

	metricsLabel_.setText("Metric", dontSendNotification);
	addAndMakeVisible(metricsLabel_);

	l2_.setButtonText("L2");
	l2_.setRadioGroupId(80981, dontSendNotification);
	l2_.setClickingTogglesState(true);
	l2_.setToggleState(true, dontSendNotification);
	l2_.onClick = [this]() {
		runSearch();
	};
	addAndMakeVisible(l2_);
	ip_.setButtonText("IP");
	ip_.setRadioGroupId(80981, dontSendNotification);
	ip_.setClickingTogglesState(true);
	ip_.onClick = [this]() {
		runSearch();
	};
	addAndMakeVisible(ip_);

	similarityLabel_.setText("Cutoff", dontSendNotification);
	addAndMakeVisible(similarityLabel_);

	similarityValue_.setTitle("Cutoff"); 
	similarityValue_.setValue(0.95);
	similarityValue_.setRange(juce::Range(0.75, 1.0), 0.01);
	similarityValue_.onValueChange = [this]() {
		runSearch();
	};
	addAndMakeVisible(similarityValue_);
}

SimilarPatchesPanel::~SimilarPatchesPanel()
{
	UIModel::instance()->currentPatch_.removeChangeListener(this);
	UIModel::instance()->databaseChanged.removeChangeListener(this);
}

void SimilarPatchesPanel::resized()
{
	auto area = getLocalBounds();
	helpText_.setBounds(area.removeFromTop(3 * LAYOUT_LINE_HEIGHT));
	auto buttonRow = area.removeFromTop(LAYOUT_BUTTON_HEIGHT + 2 * LAYOUT_INSET_NORMAL);
	auto left = buttonRow.removeFromLeft(buttonRow.getWidth() / 2);
	metricsLabel_.setBounds(left.removeFromLeft(LAYOUT_BUTTON_WIDTH_MIN));
	l2_.setBounds(left.removeFromLeft(left.getWidth()/2).withSizeKeepingCentre(LAYOUT_BUTTON_WIDTH_MIN, LAYOUT_BUTTON_HEIGHT));
	ip_.setBounds(left.withSizeKeepingCentre(LAYOUT_BUTTON_WIDTH_MIN, LAYOUT_BUTTON_HEIGHT));
	similarityLabel_.setBounds(buttonRow.removeFromLeft(LAYOUT_BUTTON_WIDTH_MIN));
	similarityValue_.setBounds(buttonRow.reduced(LAYOUT_INSET_SMALL));
	similarity_->setBounds(area.reduced(LAYOUT_INSET_NORMAL));
}

void SimilarPatchesPanel::changeListenerCallback(ChangeBroadcaster* source)
{
	if (source == &UIModel::instance()->currentPatch_)
	{
		runSearch();
	} 
	else if (source == &UIModel::instance()->databaseChanged) {
		similarity_->clearList();
	}
}

void SimilarPatchesPanel::runSearch() {
	if (UIModel::currentPatch().patch()) {
		auto metric = SimilarityMetric::IP;
		float similarityCutoff = static_cast<float>(similarityValue_.getValue());
		if (l2_.getToggleState()) {
			metric = SimilarityMetric::L2;
		}
		// Search
		auto hits = activeIndex_->findSimilarPatches(UIModel::currentPatch(), 16, metric, similarityCutoff);
		similarList_->setPatches(hits);
		similarity_->setPatchList(similarList_, buttonMode_);
	}
	else {
		similarity_->clearList();
		activeIndex_ = std::make_unique<PatchSimilarity>(db_);
	}
}