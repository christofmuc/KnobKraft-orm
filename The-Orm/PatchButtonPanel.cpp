/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "PatchButtonPanel.h"

#include "Patch.h"
#include "UIModel.h"
#include "LayeredPatchCapability.h"

#include "ColourHelpers.h"
#include "LayoutConstants.h"

#include <boost/format.hpp>
#include <algorithm>

PatchButtonPanel::PatchButtonPanel(std::function<void(midikraft::PatchHolder &)> handler) :
	handler_(handler), pageBase_(0), pageNumber_(0), pageSize_(64), totalSize_(0)
{
	// We want 64 patch buttons
	patchButtons_ = std::make_unique<PatchButtonGrid<PatchHolderButton>>(8, 8, [this](int index) { buttonClicked(index); });
	addAndMakeVisible(patchButtons_.get());

	addAndMakeVisible(pageUp_); 
	pageUp_.setButtonText(">");
	pageUp_.addListener(this);
	addAndMakeVisible(pageDown_); 
	pageDown_.setButtonText("<");
	pageDown_.addListener(this);

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

	UIModel::instance()->thumbnails_.addChangeListener(this);
}

PatchButtonPanel::~PatchButtonPanel() {
	UIModel::instance()->thumbnails_.removeChangeListener(this);
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
		if (i >= pageButtonMap_.size()) {
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
			buttonClicked(autoSelectTarget);
		}
		else if (autoSelectTarget == 1) {
			buttonClicked(((int) patches_.size()) - 1);
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

	// Check if we have a multi-synth grid, then we turn on subtitles
	std::set<std::string> synths;
	for (int i = 0; i < (int)std::min(patchButtons_->size(), patches_.size()); i++) {
		if (patches_[i].synth())
			synths.insert(patches_[i].synth()->getName());
	}
	bool showSubtitles = synths.size() > 1;

	// Now set the button text and colors
	int active = indexOfActive();
	for (int i = 0; i < (int) std::max(patchButtons_->size(), patches_.size()); i++) {
		if (i < patchButtons_->size()) {
			auto button = patchButtons_->buttonWithIndex(i);
			if (i < patches_.size()) {
				button->setPatchHolder(&patches_[i], i == active, showSubtitles);
				refreshThumbnail(i);
			}
			else {
				button->setPatchHolder(nullptr, false, showSubtitles);
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

	pageDown_.setBounds(area.removeFromLeft(LAYOUT_SMALL_ICON_WIDTH).withTrimmedRight(LAYOUT_INSET_NORMAL));
	pageUp_.setBounds(area.removeFromRight(LAYOUT_SMALL_ICON_WIDTH).withTrimmedLeft(LAYOUT_INSET_NORMAL));

	patchButtons_->setBounds(area);
}

void PatchButtonPanel::buttonClicked(int buttonIndex) {
	if (buttonIndex >= 0 && buttonIndex < (int) patches_.size()) {
		int active = indexOfActive();
		if (active != -1) {
			patchButtons_->buttonWithIndex(active)->setActive(false);
		}
		// This button is active, is it?
		handler_(patches_[buttonIndex]);
		activePatchMd5_ = patches_[buttonIndex].md5();
		patchButtons_->buttonWithIndex(buttonIndex)->setActive(true);
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
	ignoreUnused(source);
	// Some Thumbnail has changed, most likely it is visible...
	for (int i = 0; i < std::min(patchButtons_->size(), patches_.size()); i++) {
		refreshThumbnail(i);
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
		if (active + 1 < patchButtons_->size()) {
			if (active + 1 < patches_.size()) {
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
