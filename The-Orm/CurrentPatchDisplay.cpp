/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "CurrentPatchDisplay.h"

//#include "SessionDatabase.h"

CurrentPatchDisplay::CurrentPatchDisplay(std::function<void(midikraft::PatchHolder&)> favoriteHandler,
	std::function<void(midikraft::PatchHolder&)> sessionHandler) : Component(), favoriteHandler_(favoriteHandler), sessionHandler_(sessionHandler)
	, currentPatch_(nullptr), categories_({}, [this]() { categoryUpdated();  }, false),
	name_("PATCHNAME", "No patch loaded"),
	currentSession_("Current Session"), 
	favorite_("Fav!"),
	import_("IMPORT", "No import information")
{
	addAndMakeVisible(&name_);

	favorite_.setClickingTogglesState(true);
	favorite_.addListener(this);
	favorite_.setColour(TextButton::ColourIds::buttonColourId, Colours::black);
	favorite_.setColour(TextButton::ColourIds::buttonOnColourId, Colours::limegreen);
	addAndMakeVisible(favorite_);

	currentSession_.setClickingTogglesState(true);
	currentSession_.addListener(this);
	addAndMakeVisible(currentSession_);

	addAndMakeVisible(categories_);
	addAndMakeVisible(import_);
}

void CurrentPatchDisplay::setCurrentPatch(midikraft::Synth *synth, midikraft::PatchHolder *patch)
{
	if (patch) {
		name_.setText(patch->patch()->patchName(), dontSendNotification);
		if (patch->sourceInfo()) {
			import_.setText(patch->sourceInfo()->toDisplayString(synth), dontSendNotification);
		}
		else {
			import_.setText("No import information", dontSendNotification);
		}
		favorite_.setToggleState(patch->isFavorite(), dontSendNotification);
		
		std::set<CategoryButtons::Category> buttonCategories;
		for (const auto& cat : patch->categories()) {
			buttonCategories.insert({ cat.category, cat.color });
		}
		categories_.setActive(buttonCategories);
	}
	else {
		name_.setText("No patch loaded", dontSendNotification);
		import_.setText("", dontSendNotification);
		favorite_.setToggleState(false, dontSendNotification);
		categories_.setActive({});
	}
	currentPatch_ = patch;
	currentSynth_ = synth;
}

void CurrentPatchDisplay::reset()
{
	name_.setText("No patch loaded", dontSendNotification);
	favorite_.setToggleState(false, dontSendNotification);
	currentPatch_ = nullptr;

}

void CurrentPatchDisplay::resized()
{
	Rectangle<int> area(getLocalBounds());
	auto topRow = area.removeFromTop(60).reduced(10);
	favorite_.setBounds(topRow.removeFromRight(100));
	currentSession_.setBounds(topRow.removeFromRight(100));
	import_.setBounds(topRow.removeFromRight(300).withTrimmedRight(10));
	name_.setBounds(topRow.withTrimmedRight(10));
	auto bottomRow = area.removeFromTop(80).reduced(10);
	categories_.setBounds(bottomRow);
	
}

void CurrentPatchDisplay::buttonClicked(Button *button)
{
	if (button == &favorite_) {
		if (currentPatch_) {
			currentPatch_->setFavorite(midikraft::Favorite(button->getToggleState()));
			favoriteHandler_(*currentPatch_);
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

void CurrentPatchDisplay::categoryUpdated() {
	if (currentPatch_) {
		auto categories = categories_.selectedCategories();
		currentPatch_->clearCategories();
		for (const auto& cat : categories) {
			// Have to convert into juce-widget version of Category here
			midikraft::Category newCat({ cat.category, cat.color });
			currentPatch_->setCategory(newCat, true);
		}
		favoriteHandler_(*currentPatch_);
	}
}
