/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "CurrentPatchDisplay.h"

#include "PatchNameDialog.h"

//#include "SessionDatabase.h"

CurrentPatchDisplay::CurrentPatchDisplay(std::vector<CategoryButtons::Category> categories, std::function<void(midikraft::PatchHolder&)> favoriteHandler,
	std::function<void(midikraft::PatchHolder&)> sessionHandler) : Component(), favoriteHandler_(favoriteHandler), sessionHandler_(sessionHandler)
	, categories_(categories, [this](CategoryButtons::Category categoryClicked) { 
		midikraft::Category cat({ categoryClicked.category, categoryClicked.color, categoryClicked.bitIndex });
		categoryUpdated(cat);
	}, false),
	name_("PATCHNAME", "No patch loaded"),
	currentSession_("Current Session"), 
	favorite_("Fav!"),
	hide_("Hide"),
	import_("IMPORT", "No import information")
{
	name_.addListener(this);
	addAndMakeVisible(&name_);

	favorite_.setClickingTogglesState(true);
	favorite_.addListener(this);
	favorite_.setColour(TextButton::ColourIds::buttonColourId, Colours::black);
	favorite_.setColour(TextButton::ColourIds::buttonOnColourId, Colours::limegreen);
	addAndMakeVisible(favorite_);

	hide_.setClickingTogglesState(true);
	hide_.addListener(this);
	hide_.setColour(TextButton::ColourIds::buttonColourId, Colours::black);
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

void CurrentPatchDisplay::setCurrentPatch(midikraft::Synth *synth, midikraft::PatchHolder patch)
{
	if (patch.patch()) {
		name_.setButtonText(patch.patch()->patchName());
		if (patch.sourceInfo()) {
			import_.setText(patch.sourceInfo()->toDisplayString(synth), dontSendNotification);
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
	}
	else {
		name_.setButtonText("No patch loaded");
		import_.setText("", dontSendNotification);
		favorite_.setToggleState(false, dontSendNotification);
		hide_.setToggleState(false, dontSendNotification);
		categories_.setActive({});
	}
	currentPatch_ = patch;
	currentSynth_ = synth;
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
	Rectangle<int> area(getLocalBounds());
	auto topRow = area.removeFromTop(60).reduced(10);
	hide_.setBounds(topRow.removeFromRight(100));
	favorite_.setBounds(topRow.removeFromRight(100));
	//currentSession_.setBounds(topRow.removeFromRight(100));
	import_.setBounds(topRow.removeFromRight(300).withTrimmedRight(10));
	name_.setBounds(topRow.withTrimmedRight(10));
	auto bottomRow = area.removeFromTop(80).reduced(10);
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
	else if (button == &name_) {
		PatchNameDialog::showPatchNameDialog(&currentPatch_, getTopLevelComponent());
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
}
