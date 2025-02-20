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

MetaDataArea::MetaDataArea(std::vector<CategoryButtons::Category> categories, std::function<void(CategoryButtons::Category, TouchButtonFunction f)> categoryUpdateHandler) :
	categories_(categories, categoryUpdateHandler, false, false)
	, patchAsText_([this]() { if (forceResize) forceResize();  }, false)
{
	addAndMakeVisible(categories_);
	categories_.setButtonSize(LAYOUT_BUTTON_WIDTH, LAYOUT_TOUCHBUTTON_HEIGHT);
	addChildComponent(patchAsText_);
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

void MetaDataArea::setPatchText(std::shared_ptr<midikraft::PatchHolder> patch)
{
	patchAsText_.fillTextBox(patch);
	patchAsText_.setVisible(patch && patch->patch());
}

void MetaDataArea::resized()
{
	auto area = getLocalBounds();

	// Make sure our component is large enough!
	auto desiredBounds = categories_.determineSubAreaForButtonLayout(this, Rectangle<int>(0, 0, area.getWidth(), 10000));
	categories_.setBounds(area.removeFromTop((int) desiredBounds.getHeight())); 
	//categories_.setBounds(area.withTrimmedRight(LAYOUT_INSET_NORMAL)); // Leave room for the scrollbar on the right
	patchAsText_.setBounds(area.withTrimmedTop(2 * LAYOUT_INSET_NORMAL).withHeight((int) patchAsText_.desiredHeight()));
}

int MetaDataArea::getDesiredHeight(int width) 
{
	// Given the width, determine the required height of the flex box button layout
	auto desiredBounds = categories_.determineSubAreaForButtonLayout(this, Rectangle<int>(0, 0, width, 10000));
	return static_cast<int>(desiredBounds.getHeight() + patchAsText_.desiredHeight() + 2 * LAYOUT_INSET_NORMAL);
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
	, metaData_(categories, [this](CategoryButtons::Category categoryClicked, TouchButtonFunction f) {
		categoryUpdated(categoryClicked, f);
		refreshCategories();
		refreshNameButtonColour();
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

	metaData_.forceResize = [this]() {
		resized();
	};

	metaDataScroller_.setViewedComponent(&metaData_, false);
	addAndMakeVisible(metaDataScroller_);

	if (Settings::instance().keyIsSet("MetaDataLayout")) {
		lastOpenState_ = Settings::instance().get("MetaDataLayout");
	}

	// We need to recolor in case the categories are changed, or the database
	UIModel::instance()->categoriesChanged.addChangeListener(this);
	UIModel::instance()->databaseChanged.addChangeListener(this);
}

CurrentPatchDisplay::~CurrentPatchDisplay()
{
	UIModel::instance()->categoriesChanged.removeChangeListener(this);
	UIModel::instance()->databaseChanged.removeChangeListener(this);
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
		
		refreshCategories();

		metaData_.setPatchText(patch);
	}
	else {
		reset();
		currentPatch_ = patch; // Keep the patch anyway, even if it was empty
		jassertfalse;
	}

	if (lastOpenState_.isNotEmpty()) {
		propertyEditor_.fromLayout(lastOpenState_);
		lastOpenState_.clear();
	}
	resized();
}

void CurrentPatchDisplay::refreshCategories()
{
	std::set<CategoryButtons::Category> buttonCategories;
	if (currentPatch_ && currentPatch_->patch()) {
		for (const auto& cat : currentPatch_->categories()) {
			buttonCategories.insert({ cat.category(), cat.color() });
		}
	}
	metaData_.setActive(buttonCategories);
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
	layerNameValues_.clear();

	// Check if the patch is a layered patch
	auto layers = midikraft::Capability::hasCapability<midikraft::LayeredPatchCapability>(patch->patch());
	if (layers) {
		auto titles = layers->layerTitles();
		for (int i = 0; i < layers->numberOfLayers(); i++) {
			String title = "Layer " + String(i);
			if (i < (int) titles.size()) {
				title = titles[i];
			}
			TypedNamedValue v(title, "Patch name", String(layers->layerName(i)), 20);
			metaDataValues_.push_back(std::make_shared<TypedNamedValue>(v));
			layerNameValues_.push_back(metaDataValues_.back());
		}
	}
	else if (patch->patch()) {
		TypedNamedValue v("Patch name", "Patch name", String(patch->name()), 20);
		metaDataValues_.push_back(std::make_shared<TypedNamedValue>(v));
	}

	std::string knownPositions;
	auto alreadyInSynth = database_.getBankPositions(patch->smartSynth(), patch->md5());
	for (auto const& pos : alreadyInSynth) {
		if (!knownPositions.empty()) {
			knownPositions += ", ";
		}
		knownPositions += patch->synth()->friendlyProgramAndBankName(pos.isBankKnown() ? pos.bank() : MidiBankNumber::invalid(), pos);
	}
	if (knownPositions.empty()) {
		knownPositions = "no place known";
	}

	// More read only data
	auto synth = patch->smartSynth();
	metaDataValues_.push_back(std::make_shared<TypedNamedValue>("Synth", "Meta data", patch->synth()->getName(), 100));
	metaDataValues_.back()->setEnabled(false);
	metaDataValues_.push_back(std::make_shared<TypedNamedValue>("Type", "Meta data", getTypeName(patch), 100));
	metaDataValues_.back()->setEnabled(false);
	metaDataValues_.push_back(std::make_shared<TypedNamedValue>("Import", "Meta data", getImportName(patch), 100));
	metaDataValues_.back()->setEnabled(false);
	metaDataValues_.push_back(std::make_shared<TypedNamedValue>("Program", "Meta data", synth->friendlyProgramName(patch->patchNumber()), 100));
	metaDataValues_.back()->setEnabled(false);
	metaDataValues_.push_back(std::make_shared<TypedNamedValue>("In synth at", "Meta data", knownPositions, 100));
	metaDataValues_.back()->setEnabled(false);
	metaDataValues_.push_back(std::make_shared<TypedNamedValue>("Size", "Meta data", fmt::format("{} Bytes", patch->patch()->data().size()), 100));
	metaDataValues_.back()->setEnabled(false);

	// More editable data
	metaDataValues_.push_back(std::make_shared<TypedNamedValue>("Author", "Meta data", patch->author(), 256, false));
	metaDataValues_.push_back(std::make_shared<TypedNamedValue>("Info", "Meta data", patch->info(), 256, false));
	metaDataValues_.push_back(std::make_shared<TypedNamedValue>("Comment", "Meta data", patch->comment(), 2048, true));
	
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
					for (size_t i = 0; i < layerNameValues_.size(); i++) {
						if (layerNameValues_[i] == property) {
							layers->setLayerName((int) i, value.getValue().toString().toStdString());
							currentPatch_->setName(currentPatch_->name()); // We need to refresh the name in the patch holder to match the name calculated from the 2 layers!
							setCurrentPatch(currentPatch_);
							favoriteHandler_(currentPatch_);
							return;
						}
					}
					spdlog::error("Program error: Failed to find value for property {}", property->name().toStdString());
					return;
				}
			}
			else {
				jassertfalse;
			}
		}
		else if (property->name() == "Comment" && value.refersToSameSourceAs(property->value())) {
			if (currentPatch_) {
				currentPatch_->setComment(value.getValue().toString().toStdString());
				setCurrentPatch(currentPatch_);
				favoriteHandler_(currentPatch_);
				return;
			}
		}
		else if (property->name() == "Author" && value.refersToSameSourceAs(property->value())) {
			if (currentPatch_) {
				currentPatch_->setAuthor(value.getValue().toString().toStdString());
				setCurrentPatch(currentPatch_);
				favoriteHandler_(currentPatch_);
				return;
			}
		}
		else if (property->name() == "Info" && value.refersToSameSourceAs(property->value())) {
			if (currentPatch_) {
				currentPatch_->setInfo(value.getValue().toString().toStdString());
				setCurrentPatch(currentPatch_);
				favoriteHandler_(currentPatch_);
				return;
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
	layerNameValues_.clear();
	favorite_.setToggleState(false, dontSendNotification);
	hide_.setToggleState(false, dontSendNotification);
	metaData_.setActive({});
	metaData_.setPatchText(nullptr);
	resized();
}

void CurrentPatchDisplay::resized()
{	
	Rectangle<int> area(getLocalBounds().reduced(LAYOUT_INSET_NORMAL)); 

	if (area.getWidth() < area.getHeight() * 1.5) {
		// Portrait
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
	}
	else {
		// Landscape - the classical layout originally done for the Portrait Tablet

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
	if (source == &UIModel::instance()->categoriesChanged) {
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
	else if (source == &UIModel::instance()->databaseChanged)
	{
		reset();
	}
}

void CurrentPatchDisplay::categoryUpdated(CategoryButtons::Category clicked, TouchButtonFunction f) {
	if (currentPatch_ && currentPatch_->patch()) {
		auto databaseCategories = database_.getCategories();
		// Search for the real category
		for (auto realCat: databaseCategories) {
			if (realCat.category() == clicked.category) {
				currentPatch_->setUserDecision(realCat);

				std::vector<CategoryButtons::Category> categoriesToSet;
				if (f == TouchButtonFunction::PRIMARY) {
					categoriesToSet = metaData_.selectedCategories();
				}
				else if (f == TouchButtonFunction::SECONDARY) {
					// Single category
					categoriesToSet.push_back(clicked);
				}

				// Recalculate the set of categories
				currentPatch_->clearCategories();
				for (const auto& cat : categoriesToSet) {
					// Have to convert into juce-widget version of Category here
					bool found = false;
					for (auto c : databaseCategories) {
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
