/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "CurrentPatchDisplay.h"

#include "Capability.h"

#include "PatchButtonPanel.h"
#include "PatchHolderButton.h"
#include "DataFileLoadCapability.h"

#include "ColourHelpers.h"
#include "LayoutConstants.h"
#include "UIModel.h"
#include "FlexBoxHelper.h"

#include "LayeredPatchCapability.h"

#include "Settings.h"

#include <spdlog/spdlog.h>

MetaDataArea::MetaDataArea(std::vector<CategoryButtons::Category> categories, std::function<void(CategoryButtons::Category)> categoryUpdateHandler) :
	categories_(categories, categoryUpdateHandler, false, false)
{
	addAndMakeVisible(categories_);
	categories_.setButtonSize(LAYOUT_BUTTON_WIDTH, LAYOUT_TOUCHBUTTON_HEIGHT);
}

void MetaDataArea::setActive(std::set<CategoryButtons::Category> const& activeCategories) 
{
	categories_.setActive(activeCategories);
}

void MetaDataArea::setCategories(std::vector<CategoryButtons::Category> const& categories) 
{
	categories_.setCategories(categories);
}

std::vector<CategoryButtons::Category> MetaDataArea::selectedCategories() const 
{
	return categories_.selectedCategories();
}

void MetaDataArea::resized()
{
	auto area = getLocalBounds();

	// Make sure our component is large enough!
	categories_.setBounds(area); 
	//categories_.setBounds(area.withTrimmedRight(LAYOUT_INSET_NORMAL)); // Leave room for the scrollbar on the right
}

int MetaDataArea::getDesiredHeight(int width) 
{
	// Given the width, determine the required height of the flex box button layout
	auto desiredBounds = categories_.determineSubAreaForButtonLayout(this, Rectangle<int>(0, 0, width, 10000));
	return static_cast<int>(desiredBounds.getHeight());
}

CurrentPatchDisplay::CurrentPatchDisplay(midikraft::PatchDatabase &database, std::vector<CategoryButtons::Category> categories, std::function<void(std::shared_ptr<midikraft::PatchHolder>)> favoriteHandler) 
	: Component(), database_(database)
	, name_(0, false, [this](int) {
		if (onCurrentPatchClicked) {
			onCurrentPatchClicked(currentPatch_);
		}
	})
	, propertyEditor_(true)
	, favorite_("Fav!")
	, hide_("Hide")
	, metaData_(categories, [this](CategoryButtons::Category categoryClicked) {
		categoryUpdated(categoryClicked);
	})
    , favoriteHandler_(favoriteHandler)
	{
	addAndMakeVisible(&name_);
	addAndMakeVisible(&propertyEditor_);

	favorite_.setClickingTogglesState(true);
	favorite_.addListener(this);
	favorite_.setColour(TextButton::ColourIds::buttonOnColourId, Colour::fromString("ffffa500"));
	addAndMakeVisible(favorite_);

	hide_.setClickingTogglesState(true);
	hide_.addListener(this);
	hide_.setColour(TextButton::ColourIds::buttonOnColourId, Colours::indianred);
	addAndMakeVisible(hide_);

	metaDataScroller_.setViewedComponent(&metaData_, false);
	addAndMakeVisible(metaDataScroller_);
	addAndMakeVisible(patchAsText_);

	if (Settings::instance().keyIsSet("MetaDataLayout")) {
		lastOpenState_ = Settings::instance().get("MetaDataLayout");
	}

	// We need to recolor in case the categories are changed
	UIModel::instance()->categoriesChanged.addChangeListener(this);
}

CurrentPatchDisplay::~CurrentPatchDisplay()
{
	UIModel::instance()->categoriesChanged.removeChangeListener(this);
	Settings::instance().set("MetaDataLayout", propertyEditor_.getLayout().toStdString());
}

void CurrentPatchDisplay::setCurrentPatch(std::shared_ptr<midikraft::PatchHolder> patch)
{
	if (lastOpenState_.isEmpty()) {
		lastOpenState_ = propertyEditor_.getLayout();
	}
	currentPatch_ = patch;
	if (patch && patch->patch()) {
		//TODO This should use a PatchHolder and a PatchHolderButton
		name_.setButtonData(patch->name());
		name_.setButtonDragInfo(patch->createDragInfoString());
		setupPatchProperties(patch);
		refreshNameButtonColour();
		favorite_.setToggleState(patch->isFavorite(), dontSendNotification);
		hide_.setToggleState(patch->isHidden(), dontSendNotification);
		
		std::set<CategoryButtons::Category> buttonCategories;
		for (const auto& cat : patch->categories()) {
			buttonCategories.insert({ cat.category(), cat.color() });
		}
		metaData_.setActive(buttonCategories);

		if (patchAsText_.isVisible()) {
			patchAsText_.fillTextBox(patch);
		}
	}
	else {
		reset();
		currentPatch_ = patch; // Keep the patch anyway, even if it was empty
		jassertfalse;
	}

	if (lastOpenState_.isNotEmpty()) {
		propertyEditor_.fromLayout(lastOpenState_);
		lastOpenState_.clear();
		resized();
	}
}

String getTypeName(std::shared_ptr<midikraft::PatchHolder> patch)
{
	auto dataFileCap = midikraft::Capability::hasCapability<midikraft::DataFileLoadCapability>(patch->smartSynth());
	if (dataFileCap) {
		int datatype = patch->patch()->dataTypeID();
		if (datatype < static_cast<int>(dataFileCap->dataTypeNames().size())) {
			return dataFileCap->dataTypeNames()[patch->patch()->dataTypeID()].name;
		}
		else {
			return "unknown";
		}
	}
	// This without datafile capa only have patches
	return "Patch";
}

String getImportName(std::shared_ptr<midikraft::PatchHolder> patch)
{
	if (patch->sourceInfo()) {
		auto friendlyName = patch->synth()->friendlyProgramName(patch->patchNumber());
		String position = fmt::format(" at {}", friendlyName);
		return String(patch->sourceInfo()->toDisplayString(patch->synth(), false)) + position;
	}
	else
	{
		return "No import information";
	}
}


void CurrentPatchDisplay::setupPatchProperties(std::shared_ptr<midikraft::PatchHolder> patch)
{
	metaDataValues_.clear();

	// Check if the patch is a layered patch
	auto layers = midikraft::Capability::hasCapability<midikraft::LayeredPatchCapability>(patch->patch());
	if (layers) {
		for (int i = 0; i < layers->numberOfLayers(); i++) {
			TypedNamedValue v("Layer " + String(i), "Patch name", String(layers->layerName(i)).trim(), 20);
			metaDataValues_.push_back(std::make_shared<TypedNamedValue>(v));
		}
	}
	else if (patch->patch()) {
		TypedNamedValue v("Patch name", "Patch name", String(patch->name()).trim(), 20);
		metaDataValues_.push_back(std::make_shared<TypedNamedValue>(v));
	}

	// More read only data
	metaDataValues_.push_back(std::make_shared<TypedNamedValue>("Synth", "Meta data", patch->synth()->getName(), 100));
	metaDataValues_.back()->setEnabled(false);
	metaDataValues_.push_back(std::make_shared<TypedNamedValue>("Type", "Meta data", getTypeName(patch), 100));
	metaDataValues_.back()->setEnabled(false);
	metaDataValues_.push_back(std::make_shared<TypedNamedValue>("Import", "Meta data", getImportName(patch), 100));
	metaDataValues_.back()->setEnabled(false);

	// We need to learn about updates
	for (auto tnv : metaDataValues_) {
		tnv->value().addListener(this);
	}
	propertyEditor_.setProperties(metaDataValues_);
	resized();
}

void CurrentPatchDisplay::valueChanged(Value& value)
{
	for (auto property : metaDataValues_) {
		if (property->name() == "Patch name" && value.refersToSameSourceAs(property->value())) {
			// Name was changed - do this in the database!
			if (currentPatch_) {
				currentPatch_->setName(value.getValue().toString().toStdString());
				setCurrentPatch(currentPatch_);
				favoriteHandler_(currentPatch_);
				return;
			}
			else {
				jassertfalse;
			}
		}
		else if (property->name().startsWith("Layer") && value.refersToSameSourceAs(property->value())) {
			if (currentPatch_) {
				// A layer name was changed
				auto layers = midikraft::Capability::hasCapability<midikraft::LayeredPatchCapability>(currentPatch_->patch());
				if (layers) {
					int i = atoi(property->name().substring(6).toStdString().c_str());
					layers->setLayerName(i, value.getValue().toString().toStdString());
					currentPatch_->setName(currentPatch_->name()); // We need to refresh the name in the patch holder to match the name calculated from the 2 layers!
					setCurrentPatch(currentPatch_);
					favoriteHandler_(currentPatch_);
					return;
				}
			}
			else {
				jassertfalse;
			}
		}
	}
}


void CurrentPatchDisplay::reset()
{
	currentPatch_ = std::make_shared<midikraft::PatchHolder>();
	propertyEditor_.setProperties({});
	name_.setButtonData("No patch loaded");
	metaDataValues_.clear();
	favorite_.setToggleState(false, dontSendNotification);
	hide_.setToggleState(false, dontSendNotification);
	metaData_.setActive({});
	patchAsText_.fillTextBox(nullptr);
	resized();
}

void CurrentPatchDisplay::resized()
{	
	Rectangle<int> area(getLocalBounds().reduced(LAYOUT_INSET_NORMAL)); 

	if (area.getWidth() < area.getHeight() * 1.5) {
		// Portrait
		patchAsText_.setVisible(true);

		auto topRow = area.removeFromTop(LAYOUT_TOUCHBUTTON_HEIGHT);
		name_.setBounds(topRow);

		// Property Editor at the top
		int desiredHeight = propertyEditor_.getTotalContentHeight();
		auto top = area.removeFromTop(desiredHeight);
		propertyEditor_.setBounds(top);

		// Next row fav and hide
		auto nextRow = area.removeFromTop(LAYOUT_LINE_SPACING).withTrimmedTop(LAYOUT_INSET_NORMAL);
		FlexBox fb;
		fb.flexWrap = FlexBox::Wrap::wrap;
		fb.flexDirection = FlexBox::Direction::row;
		fb.justifyContent = FlexBox::JustifyContent::center;
		fb.items.add(FlexItem(favorite_).withMinHeight(LAYOUT_TOUCHBUTTON_HEIGHT).withMinWidth(LAYOUT_BUTTON_WIDTH_MIN));
		fb.items.add(FlexItem(hide_).withMinHeight(LAYOUT_TOUCHBUTTON_HEIGHT).withMinWidth(LAYOUT_BUTTON_WIDTH_MIN));
		auto spaceNeeded = FlexBoxHelper::determineSizeForButtonLayout(this, this, { &favorite_, &hide_ }, nextRow);
		fb.performLayout(spaceNeeded.toNearestInt());
		area.removeFromTop((int) spaceNeeded.getHeight());

		auto metaDataWidth = area.getWidth() - LAYOUT_INSET_NORMAL; // Allow for the vertical scrollbar on the right hand side!
		metaData_.setSize(metaDataWidth, metaData_.getDesiredHeight(metaDataWidth));
		metaDataScroller_.setBounds(area.withTrimmedTop(LAYOUT_INSET_NORMAL));

		//SimpleLogger::instance()->postMessage("Height is " + String(categories_.getChildComponent(categories_.getNumChildComponents() - 1)->getBottom()));
		// Upper 25% of rest, tag buttons
		//categories_.setBounds(area.removeFromTop(area.getHeight()/4).withTrimmedTop(LAYOUT_INSET_NORMAL));
		//patchAsText_.setBounds(area);
		patchAsText_.setVisible(false); // Feature to be enabled later
	}
	else {
		// Landscape - the classical layout originally done for the Portrait Tablet
		patchAsText_.setVisible(false);

		// Split the top row in three parts, with the centered one taking 240 px (the patch name)
		auto topRow = area.removeFromTop(LAYOUT_TOUCHBUTTON_HEIGHT);
		int side = (topRow.getWidth() - 240) / 2;
		//auto leftCorner = topRow.removeFromLeft(side).withTrimmedRight(8);
		//auto leftCornerUpper = leftCorner.removeFromTop(20);
		//auto leftCornerLower = leftCorner;
		auto rightCorner = topRow.removeFromRight(side).withTrimmedLeft(8);

		// Right side - hide and favorite button
		hide_.setBounds(rightCorner.removeFromRight(100));
		favorite_.setBounds(rightCorner.removeFromRight(100));

		//TODO - Need to setup property table

		// Center - patch name
		name_.setBounds(topRow);

		auto bottomRow = area.removeFromTop(80).withTrimmedTop(8);
		auto metaDataWidth = bottomRow.getWidth() - LAYOUT_INSET_NORMAL; // Allow for the vertical scrollbar on the right hand side!
		metaData_.setSize(metaDataWidth, metaData_.getDesiredHeight(metaDataWidth));
		metaDataScroller_.setBounds(bottomRow);
	}
}

void CurrentPatchDisplay::buttonClicked(Button *button)
{
	if (currentPatch_) {
		if (button == &favorite_) {
			if (currentPatch_->patch()) {
				currentPatch_->setFavorite(midikraft::Favorite(button->getToggleState()));
				favoriteHandler_(currentPatch_);
			}
		}
		else if (button == &hide_) {
			if (currentPatch_->patch()) {
				currentPatch_->setHidden(hide_.getToggleState());
				favoriteHandler_(currentPatch_);
			}
		}
	}
}

std::shared_ptr<midikraft::PatchHolder> CurrentPatchDisplay::getCurrentPatch() const
{
	return currentPatch_;
}

void CurrentPatchDisplay::toggleFavorite()
{
	if (currentPatch_ && currentPatch_->patch()) {
		favorite_.setToggleState(!favorite_.getToggleState(), sendNotificationAsync);
	}
}

void CurrentPatchDisplay::toggleHide()
{
	if (currentPatch_ && currentPatch_->patch()) {
		hide_.setToggleState(!hide_.getToggleState(), sendNotificationAsync);
	}
}

void CurrentPatchDisplay::refreshNameButtonColour() {
	if (currentPatch_ && currentPatch_->patch()) {
		name_.setColour(TextButton::ColourIds::buttonColourId, PatchHolderButton::buttonColourForPatch(*currentPatch_, this));
	}
	else {
		name_.setColour(TextButton::ColourIds::buttonColourId, ColourHelpers::getUIColour(this, LookAndFeel_V4::ColourScheme::widgetBackground));
	}
}

void CurrentPatchDisplay::paint(Graphics& g)
{
	g.fillAll(getLookAndFeel().findColour(TextButton::buttonOnColourId));
}

void CurrentPatchDisplay::changeListenerCallback(ChangeBroadcaster* source)
{
	ignoreUnused(source);
	std::vector<CategoryButtons::Category> result;
	for (const auto& c : database_.getCategories()) {
		if (c.def()->isActive) {
			result.emplace_back(c.category(), c.color());
		}
	}
	metaData_.setCategories(result);
	refreshNameButtonColour();
	resized();
}

void CurrentPatchDisplay::categoryUpdated(CategoryButtons::Category clicked) {
	if (currentPatch_ && currentPatch_->patch()) {
		// Search for the real category
		for (auto realCat: database_.getCategories()) {
			if (realCat.category() == clicked.category) {
				currentPatch_->setUserDecision(realCat);
				auto categories = metaData_.selectedCategories();
				currentPatch_->clearCategories();
				for (const auto& cat : categories) {
					// Have to convert into juce-widget version of Category here
					bool found = false;
					for (auto c : database_.getCategories()) {
						if (c.category() == cat.category) {
							currentPatch_->setCategory(c, true);
							found = true;
						}
					}
					if (!found) {
						spdlog::error("Can't set category {} as it is not stored in the database. Program error?", cat.category);
					}
				}
				favoriteHandler_(currentPatch_);
				break;
			}
		}
	}
	refreshNameButtonColour();
}
