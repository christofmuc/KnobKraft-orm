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
#include "PropertyEditor.h"

class MetaDataArea: public Component {
public:
	MetaDataArea(std::vector<CategoryButtons::Category> categories, std::function<void(CategoryButtons::Category)> categoryUpdateHandler);

	// Expose this functionality of the categories member
	void setActive(std::set<CategoryButtons::Category> const& activeCategories);
	void setCategories(std::vector<CategoryButtons::Category> const& categories);
	std::vector<CategoryButtons::Category> selectedCategories() const;

	virtual void resized() override;

	int getDesiredHeight(int width);

private:
	TextEditor patchDescription_;
	CategoryButtons categories_;
};

class CurrentPatchDisplay : public Component,
	private TextButton::Listener, private ChangeListener, private Value::Listener
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
	void setupPatchProperties(std::shared_ptr<midikraft::PatchHolder> patch);
	void changeListenerCallback(ChangeBroadcaster* source) override;
	void refreshNameButtonColour();
	void categoryUpdated(CategoryButtons::Category clicked);
	virtual void valueChanged(Value& value) override; // This gets called when the property editor is used

	midikraft::PatchDatabase &database_;
	PatchButton name_;
	PropertyEditor propertyEditor_;
	TextButton favorite_;
	TextButton hide_;
	Viewport metaDataScroller_;
	MetaDataArea metaData_;
	
	PatchTextBox patchAsText_;
	std::function<void(std::shared_ptr<midikraft::PatchHolder>)> favoriteHandler_;
	std::shared_ptr<midikraft::PatchHolder> currentPatch_;

	TypedNamedValueSet metaDataValues_;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CurrentPatchDisplay)
};
