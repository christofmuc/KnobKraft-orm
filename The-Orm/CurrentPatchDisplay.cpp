/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "CurrentPatchDisplay.h"

#include "PatchNameDialog.h"
#include "PatchButtonPanel.h"
#include "PatchHolderButton.h"
#include "DataFileLoadCapability.h"

#include "ColourHelpers.h"

//#include "SessionDatabase.h"

CurrentPatchDisplay::CurrentPatchDisplay(std::vector<CategoryButtons::Category> categories, std::function<void(midikraft::PatchHolder&)> favoriteHandler,
	std::function<void(midikraft::PatchHolder&)> sessionHandler) : Component(), favoriteHandler_(favoriteHandler), sessionHandler_(sessionHandler)
	, categories_(categories, [this](CategoryButtons::Category categoryClicked) { 
		midikraft::Category cat({ categoryClicked.category, categoryClicked.color, categoryClicked.bitIndex });
		categoryUpdated(cat);
	}, false, false),
	name_(0, false, [this](int) { 		
		PatchNameDialog::showPatchNameDialog(&currentPatch_, getTopLevelComponent(), [this](midikraft::PatchHolder *result) {
			setCurrentPatch(*result);
			favoriteHandler_(*result);
		}); 
	}),
	currentSession_("Current Session"), 
	favorite_("Fav!"),
	hide_("Hide"),
	import_("IMPORT", "No import information")
{
	addAndMakeVisible(synthName_);
	addAndMakeVisible(patchType_);

	addAndMakeVisible(&name_);

	favorite_.setClickingTogglesState(true);
	favorite_.addListener(this);
	favorite_.setColour(TextButton::ColourIds::buttonOnColourId, Colour::fromString("ffffa500"));
	addAndMakeVisible(favorite_);

	hide_.setClickingTogglesState(true);
	hide_.addListener(this);
	hide_.setColour(TextButton::ColourIds::buttonOnColourId, Colours::indianred);
	addAndMakeVisible(hide_);

	currentSession_.setClickingTogglesState(true);
	currentSession_.addListener(this);
	//addAndMakeVisible(currentSession_);

	addAndMakeVisible(categories_);
	addAndMakeVisible(import_);
}

CurrentPatchDisplay::~CurrentPatchDisplay()
{
	PatchNameDialog::release();
}

void CurrentPatchDisplay::setCurrentPatch(midikraft::PatchHolder patch)
{
	currentPatch_ = patch;
	if (patch.patch()) {
		name_.setButtonText(patch.name());
		refreshNameButtonColour();
		if (patch.sourceInfo()) {
			import_.setText(patch.sourceInfo()->toDisplayString(patch.synth(), false), dontSendNotification);
		}
		else {
			import_.setText("No import information", dontSendNotification);
		}
		favorite_.setToggleState(patch.isFavorite(), dontSendNotification);
		hide_.setToggleState(patch.isHidden(), dontSendNotification);
		
		std::set<CategoryButtons::Category> buttonCategories;
		for (const auto& cat : patch.categories()) {
			buttonCategories.insert({ cat.category, cat.color, cat.bitIndex });
		}
		categories_.setActive(buttonCategories);

		if (patch.synth()) {
			synthName_.setText(patch.synth()->getName(), dontSendNotification);
			auto dataFileCap = dynamic_cast<midikraft::DataFileLoadCapability *>(patch.synth());
			if (dataFileCap) {
				patchType_.setText(dataFileCap->dataTypeNames()[patch.patch()->dataTypeID()].name, dontSendNotification);
			}
			else {
				patchType_.setText("Patch", dontSendNotification);
			}
		}
		else {
			synthName_.setText("Invalid synth", dontSendNotification);
			patchType_.setText("Unknown", dontSendNotification);
		}
	}
	else {
		name_.setButtonText("No patch loaded");
		synthName_.setText("", dontSendNotification);
		patchType_.setText("", dontSendNotification);
		import_.setText("", dontSendNotification);
		favorite_.setToggleState(false, dontSendNotification);
		hide_.setToggleState(false, dontSendNotification);
		categories_.setActive({});
	}

}

void CurrentPatchDisplay::reset()
{
	name_.setButtonText("No patch loaded");
	import_.setText("", dontSendNotification);
	favorite_.setToggleState(false, dontSendNotification);
	currentPatch_ = midikraft::PatchHolder();
}

void CurrentPatchDisplay::resized()
{
	Rectangle<int> area(getLocalBounds().reduced(8)); // This is for the background colour to show more
	auto topRow = area.removeFromTop(40);

	// Split the top row in three parts, with the centered one taking 240 px (the patch name)
	int side = (topRow.getWidth() - 240) / 2;
	auto leftCorner = topRow.removeFromLeft(side).withTrimmedRight(8);
	auto leftCornerUpper = leftCorner.removeFromTop(20);
	auto leftCornerLower = leftCorner;
	auto rightCorner = topRow.removeFromRight(side).withTrimmedLeft(8);

	// Right side - hide and favorite button
	hide_.setBounds(rightCorner.removeFromRight(100));
	favorite_.setBounds(rightCorner.removeFromRight(100));

	// Left side - synth patch
	synthName_.setBounds(leftCornerUpper.removeFromLeft(100));
	patchType_.setBounds(leftCornerUpper.removeFromLeft(100).withTrimmedLeft(8));
	import_.setBounds(leftCornerLower);

	// Center - patch name
	name_.setBounds(topRow);

	auto bottomRow = area.removeFromTop(80).withTrimmedTop(8);
	categories_.setBounds(bottomRow);
	
}

void CurrentPatchDisplay::buttonClicked(Button *button)
{
	if (button == &favorite_) {
		if (currentPatch_.patch()) {
			currentPatch_.setFavorite(midikraft::Favorite(button->getToggleState()));
			favoriteHandler_(currentPatch_);
		}
	}
	else if (button == &hide_) {
		if (currentPatch_.patch()) {
			currentPatch_.setHidden(hide_.getToggleState());
			favoriteHandler_(currentPatch_);
		}
	}
	else if (button == &currentSession_) {
	/*	if (currentPatch_) {
			Session session("1");
			SessionDatabase::instance()->storePatchInSession(currentSynth_, 
				std::make_shared<SessionPatch>(session, currentSynth_->getName(), currentPatch_->patch()->patchName(), *currentPatch_));
			sessionHandler_(*currentPatch_);
		}*/
	}
}

midikraft::PatchHolder CurrentPatchDisplay::getCurrentPatch() const
{
	return currentPatch_;
}

void CurrentPatchDisplay::toggleFavorite()
{
	if (currentPatch_.patch()) {
		favorite_.setToggleState(!favorite_.getToggleState(), sendNotificationAsync);
	}
}

void CurrentPatchDisplay::toggleHide()
{
	if (currentPatch_.patch()) {
		hide_.setToggleState(!hide_.getToggleState(), sendNotificationAsync);
	}
}

void CurrentPatchDisplay::refreshNameButtonColour() {
	if (currentPatch_.patch()) {
		name_.setColour(TextButton::ColourIds::buttonColourId, PatchHolderButton::buttonColourForPatch(currentPatch_, this));
	}
	else {
		name_.setColour(TextButton::ColourIds::buttonColourId, ColourHelpers::getUIColour(this, LookAndFeel_V4::ColourScheme::widgetBackground));
	}
}

void CurrentPatchDisplay::paint(Graphics& g)
{
	g.fillAll(getLookAndFeel().findColour(TextButton::buttonOnColourId));
}

void CurrentPatchDisplay::categoryUpdated(midikraft::Category clicked) {
	if (currentPatch_.patch()) {
		currentPatch_.setUserDecision(clicked);
		auto categories = categories_.selectedCategories();
		currentPatch_.clearCategories();
		for (const auto& cat : categories) {
			// Have to convert into juce-widget version of Category here
			midikraft::Category newCat(cat.category, cat.color, cat.bitIndex);
			currentPatch_.setCategory(newCat, true); 
		}
		favoriteHandler_(currentPatch_);
	}
	refreshNameButtonColour();
}
