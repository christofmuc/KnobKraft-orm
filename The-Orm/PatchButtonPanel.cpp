/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "PatchButtonPanel.h"

#include "Patch.h"
#include "UIModel.h"

#include "ColourHelpers.h"

#include <boost/format.hpp>
#include <algorithm>

PatchButtonPanel::PatchButtonPanel(std::function<void(midikraft::PatchHolder &)> handler) :
	handler_(handler), pageBase_(0), pageNumber_(0), pageSize_(64), totalSize_(0)
{
	// We want 64 patch buttons
	patchButtons_ = std::make_unique<PatchButtonGrid>(8, 8, [this](int index) { buttonClicked(index); });
	addAndMakeVisible(patchButtons_.get());

	addAndMakeVisible(pageUp_); 
	pageUp_.setButtonText(">");
	pageUp_.addListener(this);
	addAndMakeVisible(pageDown_); 
	pageDown_.setButtonText("<");
	pageDown_.addListener(this);
	addAndMakeVisible(pageNumbers_);
	pageNumbers_.setJustificationType(Justification::centred);

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
}

String PatchButtonPanel::createNameOfThubnailCacheFile(midikraft::PatchHolder const &patch) {
	auto md5 = midikraft::PatchHolder::calcMd5(UIModel::currentSynth(), patch.patch());
	File thumbnailCache = UIModel::getThumbnailDirectory().getChildFile(md5 + ".kkc");
	return thumbnailCache.getFullPathName();
}

File PatchButtonPanel::findPrehearFile(midikraft::PatchHolder const &patch) {
	auto md5 = midikraft::PatchHolder::calcMd5(UIModel::currentSynth(), patch.patch());

	// First check the cache
	File thumbnailCache(createNameOfThubnailCacheFile(patch));
	if (thumbnailCache.existsAsFile()) {
		return thumbnailCache;
	}

	File prehear = UIModel::getPrehearDirectory().getChildFile(md5 + ".wav");
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

Colour PatchButtonPanel::buttonColourForPatch(midikraft::PatchHolder &patch, Component *componentForDefaultBackground) {
	Colour color = ColourHelpers::getUIColour(componentForDefaultBackground, LookAndFeel_V4::ColourScheme::widgetBackground);
	auto cats = patch.categories();
	if (!cats.empty()) {
		// Random in case the patch has multiple categories
		color = cats.cbegin()->color.darker();
	}
	return color;
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
			patchButtons_->buttonWithIndex(i)->setActive(i == active);
			Colour color = ColourHelpers::getUIColour(this, LookAndFeel_V4::ColourScheme::widgetBackground);
			if (i < patches_.size()) {
				button->setButtonText(patches_[i].name());
				button->setSubtitle((showSubtitles && patches_[i].synth()) ? patches_[i].synth()->getName() :  "");
				button->setColour(TextButton::ColourIds::buttonColourId, buttonColourForPatch(patches_[i], this));
				button->setFavorite(patches_[i].isFavorite());
				button->setHidden(patches_[i].isHidden());
				refreshThumbnail(i);
				}
			else {
				button->setButtonText("");
				button->setSubtitle("");
				button->setColour(TextButton::ColourIds::buttonColourId, color);
				button->setFavorite(false);
				button->setHidden(false);
				button->setHidden(false);
				button->clearThumbnailFile();
			}
		}
	}

	// Also, set the page number stripe
	std::string pages;
	size_t numberOfPages = (totalSize_ / pageSize_) + 1;
	for (size_t i = 0; i < numberOfPages; i++) {
		if ((i == numberOfPages - 1) && (totalSize_ % pageSize_ == 0)) continue;
		if (!pages.empty()) pages.append(" ");
		if (i == pageNumber_) {
			pages.append((boost::format("<%d>") % (i + 1)).str());
		}
		else {
			pages.append((boost::format("%d") % (i + 1)).str());
		}
	}
	pageNumbers_.setText(pages, dontSendNotification);
}

void PatchButtonPanel::resized()
{
	// Create 64 patch buttons in a grid
	/*FlexBox fb;
	fb.flexWrap = FlexBox::Wrap::wrap;
	fb.justifyContent = FlexBox::JustifyContent::spaceBetween;
	fb.alignContent = FlexBox::AlignContent::stretch;
	for (auto patchbutton : patchButtons_) {
		fb.items.add(FlexItem(*patchbutton).withMinWidth(50.0f).withMinHeight(50.0f));
	}
	fb.performLayout(getLocalBounds().toFloat());*/
	Rectangle<int> area(getLocalBounds());
	pageNumbers_.setBounds(area.removeFromBottom(20));
	pageDown_.setBounds(area.removeFromLeft(32).withTrimmedRight(8));
	pageUp_.setBounds(area.removeFromRight(32).withTrimmedLeft(8));

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
		refresh(true, selectNext ? 0 : -1);
	}
}

void PatchButtonPanel::pageDown(bool selectLast) {
	if (pageBase_ - pageSize_ >= 0) {
		pageBase_ -= pageSize_;
		pageNumber_--;
		refresh(true, selectLast ? 1 : -1);
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
