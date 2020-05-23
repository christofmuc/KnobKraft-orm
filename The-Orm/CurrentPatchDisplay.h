/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "JuceHeader.h"

#include "CategoryButtons.h"
#include "PatchButton.h"

#include "Patch.h"
#include "PatchHolder.h"

class CurrentPatchDisplay : public Component,
	private TextButton::Listener
{
public:
	CurrentPatchDisplay(std::vector<CategoryButtons::Category>  categories, 
		std::function<void(midikraft::PatchHolder&)> favoriteHandler, 
		std::function<void(midikraft::PatchHolder&)> sessionHandler);
	virtual ~CurrentPatchDisplay();

	void setCurrentPatch(midikraft::PatchHolder patch);
	void reset();

	void resized() override;
	void buttonClicked(Button*) override;

	midikraft::PatchHolder getCurrentPatch() const;

	// For remote control via MidiKeyboard
	void toggleFavorite();
	void toggleHide();

	// Override to allow custom colour
	virtual void paint(Graphics& g) override;

private:
	void refreshNameButtonColour();
	void categoryUpdated(midikraft::Category clicked);

	Label synthName_;
	Label patchType_;
	PatchButton name_;
	Label import_;
	TextButton currentSession_;
	TextButton favorite_;
	TextButton hide_;
	CategoryButtons categories_;
	std::function<void(midikraft::PatchHolder&)> favoriteHandler_;
	std::function<void(midikraft::PatchHolder&)> sessionHandler_;
	midikraft::PatchHolder currentPatch_;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CurrentPatchDisplay)
};
