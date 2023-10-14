/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "PatchButtonPanel.h"

#include "Patch.h"
#include "UIModel.h"

#include "ColourHelpers.h"
#include "LayoutConstants.h"
#include "Settings.h"

#include <algorithm>
#include <fmt/format.h>

PatchButtonPanel::PatchButtonPanel(std::function<void(midikraft::PatchHolder &)> handler, std::string const& settingPrefix) :
	settingPrefix_(settingPrefix), handler_(handler), pageBase_(0), pageNumber_(0), totalSize_(0)
{
	gridSizeSliderX_.setTextBoxStyle(Slider::NoTextBox, false, 0, 0);
	gridSizeSliderX_.setRange(4.0, 16.0, 1.0);
	gridSizeSliderY_.setTextBoxStyle(Slider::NoTextBox, false, 0, 0);
	gridSizeSliderY_.setRange(4.0, 10.0, 1.0);
	gridWidth_ = 8;
	gridHeight_ = 8;
	gridSizeSliderX_.setValue(8.0, dontSendNotification);
	gridSizeSliderY_.setValue(8.0, dontSendNotification);

	// Load the last size of the slider position
	if (UIModel::currentSynth()) {
		auto sliderX = Settings::instance().get(settingName(SliderAxis::X_AXIS), 8);
		auto sliderY = Settings::instance().get(settingName(SliderAxis::Y_AXIS), 8);
		if (sliderX != 0) {
			gridWidth_ = sliderX;
			gridSizeSliderX_.setValue(sliderX, dontSendNotification);
		}
		if (sliderY != 0) {
			gridHeight_ = sliderY;
			gridSizeSliderY_.setValue(sliderY, dontSendNotification);
		}
	}
	pageSize_ = gridWidth_ * gridHeight_;

	sliderXLabel_.setText("X", dontSendNotification);
	addAndMakeVisible(sliderXLabel_);
	sliderXLabel_.attachToComponent(&gridSizeSliderX_, true);
	sliderYLabel_.setText("Y", dontSendNotification);
	addAndMakeVisible(sliderYLabel_);
	sliderYLabel_.attachToComponent(&gridSizeSliderY_, true);

	patchButtons_ = std::make_unique<PatchButtonGrid<PatchHolderButton>>(gridWidth_, gridHeight_, [this](int index) { buttonClicked(index, true); });
	addAndMakeVisible(patchButtons_.get());

	addAndMakeVisible(pageUp_); 
	pageUp_.setButtonText(">");
	pageUp_.addListener(this);
	addAndMakeVisible(pageDown_); 
	pageDown_.setButtonText("<");
	pageDown_.addListener(this);

	gridSizeSliderX_.onValueChange = [this]() {
		int newX = (int) gridSizeSliderX_.getValue();
		if (UIModel::currentSynth()) {
			Settings::instance().set(settingName(SliderAxis::X_AXIS), String(newX).toStdString());
		}
		this->changeGridSize(newX, gridHeight_);
	};
	gridSizeSliderY_.onValueChange = [this]() {
		int newY = (int)gridSizeSliderY_.getValue();
		if (UIModel::currentSynth()) {
			Settings::instance().set(settingName(SliderAxis::Y_AXIS), String(newY).toStdString());
		}
		this->changeGridSize(gridWidth_, newY);
	};
	addAndMakeVisible(gridSizeSliderX_);
	addAndMakeVisible(gridSizeSliderY_);

	for (int i = 0; i < 2; i++) {
		auto e = new Label();
		e->setText("...", dontSendNotification);
		addAndMakeVisible(e);
		ellipsis_.add(std::move(e));
	}

	maxPageButtons_ = 16; // For now, hard-coded value.Should probably calculate this from the width available.
	pageNumbers_.clear();
	for (int i = 0; i < maxPageButtons_; i++) {
		TextButton *b = new TextButton();
		b->setClickingTogglesState(true);
		b->setRadioGroupId(1357);
		b->setConnectedEdges(TextButton::ConnectedOnLeft | TextButton::ConnectedOnRight);
		b->setColour(ComboBox::outlineColourId, ColourHelpers::getUIColour(this, LookAndFeel_V4::ColourScheme::windowBackground));
		addAndMakeVisible(b);
		b->setVisible(false);
		pageNumbers_.add(std::move(b));
	}

	UIModel::instance()->currentSynth_.addChangeListener(this);
	UIModel::instance()->thumbnails_.addChangeListener(this);
	UIModel::instance()->multiMode_.addChangeListener(this);
}

PatchButtonPanel::~PatchButtonPanel()
{
	UIModel::instance()->currentSynth_.removeChangeListener(this);
	UIModel::instance()->thumbnails_.removeChangeListener(this);
	UIModel::instance()->multiMode_.removeChangeListener(this);
}

std::string PatchButtonPanel::settingName(SliderAxis axis)
{
	std::string axisName = axis == SliderAxis::Y_AXIS ? "Y" : "X";
	if (settingPrefix_.empty()) {
		if (UIModel::instance()->currentSynth() == nullptr || (UIModel::instance()->multiMode_.multiSynthMode())) {
			return fmt::format("gridSizeSlider{}", axisName);
		}
		else {
			return fmt::format("{}-gridSizeSlider{}", UIModel::currentSynth()->getName(), axisName);
		}
	}
	else {
		return fmt::format("{}-gridSizeSlider{}", settingPrefix_, axisName);
	}
}

void PatchButtonPanel::refreshGridSize()
{
	if (UIModel::currentSynth()) {
		int newX = Settings::instance().get(settingName(SliderAxis::X_AXIS), 8);
		int newY = Settings::instance().get(settingName(SliderAxis::Y_AXIS), 8);
		changeGridSize(newX, newY);
		gridSizeSliderX_.setValue(newX, dontSendNotification);
		gridSizeSliderY_.setValue(newY, dontSendNotification);
	}
	else {
		changeGridSize(8, 8);
	}
}

void PatchButtonPanel::setPatchLoader(TPageLoader pageGetter)
{
	pageLoader_ = pageGetter;
}

void PatchButtonPanel::setTotalCount(int totalCount)
{
	pageBase_ = pageNumber_ = 0;
	totalSize_ = totalCount;
	numPages_ = totalCount / pageSize_;
	if (totalCount % pageSize_ != 0) numPages_++;
}

void PatchButtonPanel::changeGridSize(int newWidth, int newHeight) {
	// Remove old patch grid
	removeChildComponent(patchButtons_.get());

	// Install new patch grid
	gridWidth_ = newWidth;
	gridHeight_ = newHeight;
	pageSize_ = gridWidth_ * gridHeight_;
	numPages_ = totalSize_ / pageSize_;
	if (totalSize_ % pageSize_ != 0) numPages_++;
	patchButtons_ = std::make_unique<PatchButtonGrid<PatchHolderButton>>(gridWidth_, gridHeight_, [this](int index) { buttonClicked(index, true); });
	addAndMakeVisible(patchButtons_.get());

	resized();
	refresh(true, indexOfActive());
}

void PatchButtonPanel::setupPageButtons() {
	pageButtonMap_.clear();
	if (numPages_ <= maxPageButtons_) {
		// Easy, just make on page button for each page available
		for (int i = 0; i < numPages_; i++) {
			pageButtonMap_[i] = i;
		}
	}
	else {
		// Need first page, and then 5 more centered around the current page, and the last one to also show the count
		int button = 0;
		pageButtonMap_[button++] = 0;
		int blockStart = std::max(1, std::min(numPages_ -  6, pageNumber_ - 2));
		for (int page = blockStart; page < blockStart + 5; page++) {
			if (page > 0 && page < numPages_) {
				pageButtonMap_[button++] = page;
			}
		}
		pageButtonMap_[button++] = numPages_ - 1;
	}

	// Now relabel!
	for (auto button : pageButtonMap_) {
		Button *b = pageNumbers_[button.first];
		b->onClick = [this, b, button]() { if (b->getToggleState()) jumpToPage(button.second);  };
		b->setButtonText(String(button.second + 1));
		b->setVisible(true);
		if (button.second == pageNumber_) {
			b->setToggleState(true, dontSendNotification);
		}
	}
	// Any more buttons unused?
	for (int i = 0; i < maxPageButtons_; i++) {
		if (static_cast<size_t>(i) >= pageButtonMap_.size()) {
			pageNumbers_[i]->setVisible(false);
		}
	}
	resized();
}

void PatchButtonPanel::setPatches(std::vector<midikraft::PatchHolder> const &patches, int autoSelectTarget /* = -1 */) {
	patches_ = patches;
	// This is never an async refresh, as we might be just processing the result of an async operation, and then we'd go into a loop
	refresh(false);
	if (autoSelectTarget != -1) {
		if (autoSelectTarget == 0) {
			buttonClicked(autoSelectTarget, false);
		}
		else if (autoSelectTarget == 1) {
			buttonClicked(((int) patches_.size()) - 1, false);
		}
	}
	setupPageButtons();
}

String PatchButtonPanel::createNameOfThubnailCacheFile(midikraft::PatchHolder const &patch) {
	File thumbnailCache = UIModel::getThumbnailDirectory().getChildFile(patch.md5() + ".kkc");
	return thumbnailCache.getFullPathName();
}

File PatchButtonPanel::findPrehearFile(midikraft::PatchHolder const &patch) {
	// Check we are not too early or there is no patch to lookup
	if (!UIModel::currentSynth()) return File();
	if (!patch.patch()) return File();

	// First check the cache
	File thumbnailCache(createNameOfThubnailCacheFile(patch));
	if (thumbnailCache.existsAsFile()) {
		return thumbnailCache;
	}

	File prehear = UIModel::getPrehearDirectory().getChildFile(patch.md5() + ".wav");
	if (prehear.existsAsFile()) {
		return prehear;
	}
	return File();
}

void PatchButtonPanel::refreshThumbnail(int i) {
	File thumbnail = findPrehearFile(patches_[i]);
	if (thumbnail.existsAsFile()) {
		if (thumbnail.getFileExtension() == ".wav") {
			patchButtons_->buttonWithIndex(i)->setThumbnailFile(thumbnail.getFullPathName().toStdString(), createNameOfThubnailCacheFile(patches_[i]).toStdString());
		}
		else {
			patchButtons_->buttonWithIndex(i)->setThumbnailFromCache(Thumbnail::loadCacheInfo(thumbnail));
		}
	}
	else {
		patchButtons_->buttonWithIndex(i)->clearThumbnailFile();
	}
}

void PatchButtonPanel::refresh(bool async, int autoSelectTarget /* = -1 */) {
	if (pageLoader_ && async) {
		// If a page loader was set, we will query the current page
		pageLoader_(pageBase_, pageSize_, [this, autoSelectTarget](std::vector<midikraft::PatchHolder> const &patches) {
 			setPatches(patches, autoSelectTarget);
		});
		return;
	}

	bool multiSynthMode = UIModel::instance()->multiMode_.multiSynthMode();

	// Now set the button text and colors
	for (size_t i = 0; i < std::max(patchButtons_->size(), patches_.size()); i++) {
		if (i < patchButtons_->size()) {
			auto button = patchButtons_->buttonWithIndex((int) i);
			if (i < patches_.size() && patches_[i].patch() && patches_[i].synth()) {
				auto displayMode = PatchHolderButton::getCurrentInfoForSynth(patches_[i].synth()->getName());
				if (multiSynthMode) {
					displayMode = static_cast<PatchButtonInfo>(
						static_cast<int>(PatchButtonInfo::SubtitleSynth) | (static_cast<int>(displayMode) & static_cast<int>(PatchButtonInfo::CenterMask))
						);
				}
				button->setPatchHolder(&patches_[i], displayMode);
				refreshThumbnail((int)i);
			}
			else {
				button->setPatchHolder(nullptr, PatchButtonInfo::CenterName);
				button->clearThumbnailFile();
			}
		}
	}
}

void PatchButtonPanel::resized()
{
	Rectangle<int> area(getLocalBounds());
	auto pageNumberStrip = area.removeFromBottom(LAYOUT_LINE_SPACING).withTrimmedTop(LAYOUT_INSET_NORMAL);
	FlexBox pageNumberBox;
	pageNumberBox.flexDirection = FlexBox::Direction::row;
	pageNumberBox.justifyContent = FlexBox::JustifyContent::center;
	pageNumberBox.alignContent = FlexBox::AlignContent::center;
	int ecounter = 0;
	for (int i = 0; i < ellipsis_.size(); i++) ellipsis_[i]->setVisible(false);
	for (int i = 0; i < pageNumbers_.size(); i++) {
		auto page = pageNumbers_[i];
		if (page->isVisible()) {
			if (i > 0 && pageButtonMap_[i] != pageButtonMap_[i - 1] + 1) {
				pageNumberBox.items.add(FlexItem(*ellipsis_[ecounter]).withHeight(LAYOUT_SMALL_ICON_HEIGHT).withWidth(LAYOUT_SMALL_ICON_WIDTH));
				ellipsis_[ecounter++]->setVisible(true);
			}
			pageNumberBox.items.add(FlexItem(*page).withHeight(LAYOUT_LINE_HEIGHT).withWidth((float)page->getBestWidthForHeight(LAYOUT_LINE_HEIGHT)));
		}
	}
	pageNumberBox.performLayout(pageNumberStrip);
	gridSizeSliderY_.setBounds(pageNumberStrip.removeFromRight(LAYOUT_BUTTON_WIDTH + LAYOUT_SMALL_ICON_WIDTH));
	gridSizeSliderX_.setBounds(pageNumberStrip.withTrimmedRight(LAYOUT_SMALL_ICON_WIDTH).removeFromRight(LAYOUT_BUTTON_WIDTH + LAYOUT_SMALL_ICON_WIDTH));

	pageDown_.setBounds(area.removeFromLeft(LAYOUT_SMALL_ICON_WIDTH + LAYOUT_INSET_NORMAL).withTrimmedRight(LAYOUT_INSET_NORMAL));
	pageUp_.setBounds(area.removeFromRight(LAYOUT_SMALL_ICON_WIDTH + LAYOUT_INSET_NORMAL).withTrimmedLeft(LAYOUT_INSET_NORMAL));

	patchButtons_->setBounds(area);
}

void PatchButtonPanel::buttonClicked(int buttonIndex, bool triggerHandler) {
	if (buttonIndex >= 0 && buttonIndex < (int) patches_.size()) {
		int active = indexOfActive();
		if (active != -1) {
			patchButtons_->buttonWithIndex(active)->setActive(false);
		}
		// This button is active, is it?
		if (triggerHandler) {
			handler_(patches_[buttonIndex]);
			activePatchMd5_ = patches_[buttonIndex].md5();
			patchButtons_->buttonWithIndex(buttonIndex)->setActive(true);
		}
	}
}

void PatchButtonPanel::buttonClicked(Button* button)
{
	if (button == &pageUp_) {
		pageUp(false);
	}
	else if (button == &pageDown_) {
		pageDown(false);
	}
}

void PatchButtonPanel::pageUp(bool selectNext) {
	if (pageBase_ + pageSize_ < totalSize_) {
		pageBase_ += pageSize_;
		pageNumber_++;
		setupPageButtons();
		refresh(true, selectNext ? 0 : -1);
	}
}

void PatchButtonPanel::pageDown(bool selectLast) {
	if (pageBase_ - pageSize_ >= 0) {
		pageBase_ -= pageSize_;
		pageNumber_--;
		setupPageButtons();
		refresh(true, selectLast ? 1 : -1);
	}
}

void PatchButtonPanel::jumpToPage(int pagenumber) {
	if (pagenumber >= 0 && pagenumber < numPages_) {
		pageBase_ = pagenumber * pageSize_;
		pageNumber_ = pagenumber;
		setupPageButtons();
		refresh(true);
	}
}

void PatchButtonPanel::changeListenerCallback(ChangeBroadcaster* source)
{
	if (source == &UIModel::instance()->thumbnails_) {
		// Some Thumbnail has changed, most likely it is visible...
		for (size_t i = 0; i < std::min(patchButtons_->size(), patches_.size()); i++) {
			refreshThumbnail((int)i);
		}
	}
	else if (source == &UIModel::instance()->currentSynth_ || source == &UIModel::instance()->multiMode_) {
		refreshGridSize();
	}
}

void PatchButtonPanel::selectPrevious()
{
	int active = indexOfActive();
	if (active != -1) {
		if (active - 1 >= 0) {
			patchButtons_->buttonWithIndex(active - 1)->buttonClicked(nullptr);
		}
		else {
			pageDown(true);
		}
	}
}

void PatchButtonPanel::selectNext()
{
	int active = indexOfActive();
	if (active != -1) {
		if (active + 1 < static_cast<int>(patchButtons_->size())) {
			if (active + 1 < static_cast<int>(patches_.size())) {
				patchButtons_->buttonWithIndex(active + 1)->buttonClicked(nullptr);
			}
		}
		else {
			pageUp(true);
		}
	}
}

void PatchButtonPanel::selectFirst()
{
	pageBase_ = 0;
	pageNumber_ = 0;
	if (!pageNumbers_.isEmpty()) pageNumbers_[0]->setToggleState(true, dontSendNotification);
	refresh(true, 0);
}

int PatchButtonPanel::indexOfActive() const
{
	for (int i = 0; i < (int) patches_.size(); i++) {
		if (patches_[i].md5() == activePatchMd5_) {
			return i;
		}
	}
	return -1;
}
