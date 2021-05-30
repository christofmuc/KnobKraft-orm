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
#include "PatchTextBox.h"
#include "PatchDatabase.h"

class CurrentPatchDisplay : public Component,
	private TextButton::Listener, private ChangeListener
{
public:
	CurrentPatchDisplay(midikraft::PatchDatabase &database,
		std::vector<CategoryButtons::Category>  categories, 
		std::function<void(std::shared_ptr<midikraft::PatchHolder>)> favoriteHandler);
	virtual ~CurrentPatchDisplay();

	void setCurrentPatch(std::shared_ptr<midikraft::PatchHolder> patch);
	void reset();

	void resized() override;
	void buttonClicked(Button*) override;

	std::shared_ptr<midikraft::PatchHolder> getCurrentPatch() const;

	// For remote control via MidiKeyboard
	void toggleFavorite();
	void toggleHide();

	// Override to allow custom colour
	virtual void paint(Graphics& g) override;

private:
	void changeListenerCallback(ChangeBroadcaster* source) override;
	void refreshNameButtonColour();
	void categoryUpdated(CategoryButtons::Category clicked);

	midikraft::PatchDatabase &database_;
	Label synthName_;
	Label patchType_;
	PatchButton name_;
	Label import_;
	TextButton currentSession_;
	TextButton favorite_;
	TextButton hide_;
	CategoryButtons categories_;
	PatchTextBox patchAsText_;
	std::function<void(std::shared_ptr<midikraft::PatchHolder>)> favoriteHandler_;
	std::shared_ptr<midikraft::PatchHolder> currentPatch_;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CurrentPatchDisplay)
};
