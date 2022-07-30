/*
   Copyright (c) 2021 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "PatchSearchComponent.h"

#include "LayoutConstants.h"
#include "FlexBoxHelper.h"

#include "UIModel.h"

namespace {

	//const char* kAllDataTypesFilter = "All types";

	std::vector<std::pair<String, int>> kDisplayChoices = {
		{ "Name and #", static_cast<int>(PatchButtonInfo::NameDisplay)},
		{ "Name", static_cast<int>(PatchButtonInfo::NameDisplay) & static_cast<int>(PatchButtonInfo::CenterMask)},
		{ "Program #", static_cast<int>(PatchButtonInfo::ProgramDisplay)},
		{ "Layers and #", static_cast<int>(PatchButtonInfo::LayerDisplay)},
	};

	// A little helper
	/*CategoryButtons::Category synthCategory(midikraft::NamedDeviceCapability* name) {
		return CategoryButtons::Category(name->getName(), Colours::black);
	}*/

	std::map<std::string, std::weak_ptr<midikraft::Synth>> allSynthsMap() {
		std::map<std::string, std::weak_ptr<midikraft::Synth>> synthMap;
		for (auto synth : UIModel::instance()->synthList_.activeSynths()) {
			if (synth) {
				midikraft::SynthHolder synthFound = UIModel::instance()->synthList_.synthByName(synth->getName());
				if (synthFound.synth())
					synthMap[synth->getName()] = synthFound.synth();
			}
		}
		return synthMap;
	}

}


class AdvancedFilterPanel : public Component {
public:
	AdvancedFilterPanel(PatchView* patchView) :
		synthFilters_({}, [patchView](CategoryButtons::Category) { 
		patchView->retrieveFirstPageFromDatabase(); 
	}, false, true)
	{
		addAndMakeVisible(synthFilters_);
		addAndMakeVisible(dataTypeSelector_);
		dataTypeSelector_.setTextWhenNoChoicesAvailable("This synth does not support different data types");
		dataTypeSelector_.setTextWhenNothingSelected("Click here to show only data of a specific type");
		dataTypeSelector_.onChange = [patchView]() { 
			patchView->retrieveFirstPageFromDatabase();  
		};
	}

	void resized()
	{
		auto area = getLocalBounds();
		int width = area.getWidth() / 4;
		auto leftArea = area.removeFromLeft(width);
		synthFilters_.setBounds(area.withTrimmedLeft(LAYOUT_INSET_NORMAL));
		dataTypeSelector_.setBounds(leftArea.removeFromTop(synthFilters_.usedHeight()).withSizeKeepingCentre(width, LAYOUT_LINE_HEIGHT));
	}

	ComboBox dataTypeSelector_;
	CategoryButtons synthFilters_;
};


PatchSearchComponent::PatchSearchComponent(PatchView* patchView, PatchButtonPanel* patchButtons, midikraft::PatchDatabase& database) :
	patchView_(patchView),
	patchButtons_(patchButtons),
	database_(database),
	categoryFilters_({}, [this](CategoryButtons::Category) { updateCurrentFilter(); patchView_->retrieveFirstPageFromDatabase(); }, true, true),
	textSearch_([this]() { updateCurrentFilter(); patchView_->retrieveFirstPageFromDatabase();  })

{
	textSearch_.setFontSize(LAYOUT_LARGE_FONT_SIZE);

	onlyFaves_.setButtonText("Only Faves");
	onlyFaves_.onClick = [this]() { updateCurrentFilter(); patchView_->retrieveFirstPageFromDatabase();  };
	addAndMakeVisible(onlyFaves_);

	showHidden_.setButtonText("Also Hidden");
	showHidden_.onClick = [this]() { updateCurrentFilter(); patchView_ ->retrieveFirstPageFromDatabase();  };
	addAndMakeVisible(showHidden_);

	onlyUntagged_.setButtonText("Only Untagged");
	onlyUntagged_.onClick = [this]() { updateCurrentFilter(); patchView_->retrieveFirstPageFromDatabase();  };
	addAndMakeVisible(onlyUntagged_);

	onlyDuplicates_.setButtonText("Only duplicate Names");
	onlyDuplicates_.onClick = [this]() { updateCurrentFilter(); patchView_->retrieveFirstPageFromDatabase(); };
	addAndMakeVisible(onlyDuplicates_);

	andCategories_.setButtonText("All must match");
	andCategories_.onClick = [this]() {updateCurrentFilter(); patchView_->retrieveFirstPageFromDatabase(); };
	addAndMakeVisible(andCategories_);

	addAndMakeVisible(categoryFilters_);
	addAndMakeVisible(textSearch_);
	addAndMakeVisible(patchButtons_);

	int id = 1;
	for (auto displayChoice : kDisplayChoices) {
		buttonDisplayType_.addItem(displayChoice.first, id++);
	}
	buttonDisplayType_.setTextWhenNothingSelected("Choose button info");
	buttonDisplayType_.onChange = [this]() {
		auto synthName = currentSynthNameWithMulti();
		int selected = buttonDisplayType_.getSelectedId() - 1;
		if (selected >= 0 && selected < kDisplayChoices.size()) {
			PatchHolderButton::setCurrentInfoForSynth(synthName, static_cast<PatchButtonInfo>(kDisplayChoices[selected].second));
		}
		else {
			jassertfalse;
		}
		patchView_->retrieveFirstPageFromDatabase();
	};
	addAndMakeVisible(buttonDisplayType_);

	// Need to initialize multiModeFilter, else we get weird search results
	multiModeFilter_ = midikraft::PatchDatabase::allPatchesFilter({});

	UIModel::instance()->currentSynth_.addChangeListener(this);
	UIModel::instance()->multiMode_.addChangeListener(this);
	UIModel::instance()->synthList_.addChangeListener(this);
	UIModel::instance()->categoriesChanged.addChangeListener(this);
}

PatchSearchComponent::~PatchSearchComponent()
{
	UIModel::instance()->categoriesChanged.removeChangeListener(this);
	UIModel::instance()->currentSynth_.removeChangeListener(this);
	UIModel::instance()->multiMode_.removeChangeListener(this);
	UIModel::instance()->synthList_.removeChangeListener(this);
}

std::string PatchSearchComponent::currentSynthNameWithMulti() {
	if (isInMultiSynthMode()) return "MultiSynth";
	if (!UIModel::currentSynth()) return "none";
	return UIModel::currentSynth()->getName();
}

FlexItem createFlexButton(ToggleButton *button) {
	button->setSize(LAYOUT_CHECKBOX_WIDTH, LAYOUT_LINE_HEIGHT);
	button->changeWidthToFitText();
	return FlexItem(*button).withMinWidth((float)button->getWidth() + 20.0f).withMinHeight(LAYOUT_LINE_HEIGHT).withMargin(LAYOUT_INSET_SMALL);
}

void PatchSearchComponent::resized()
{
	Rectangle<int> area(getLocalBounds());

	// The left part with the search box gets 25% of the screen
	int leftPart = area.getWidth() / 4;

	// Determine the reserved place for the filter
	auto sortAndDisplayTypeWidth = LAYOUT_BUTTON_WIDTH + LAYOUT_INSET_NORMAL;
	auto normalFilter = area.withTrimmedLeft(LAYOUT_INSET_NORMAL).withTrimmedRight(LAYOUT_INSET_NORMAL).withTrimmedTop(LAYOUT_INSET_NORMAL);
	auto sortAndDisplayTypeArea = normalFilter.removeFromRight(sortAndDisplayTypeWidth).withTrimmedLeft(LAYOUT_INSET_NORMAL);

	auto leftHalf = normalFilter.removeFromLeft(leftPart);
	
	FlexBox fb;
	fb.flexWrap = FlexBox::Wrap::wrap;
	fb.flexDirection = FlexBox::Direction::row;
	fb.justifyContent = FlexBox::JustifyContent::center;
	fb.alignContent = FlexBox::AlignContent::flexStart; // This is cross axis, up
	fb.items.add(createFlexButton(&onlyFaves_));
	fb.items.add(createFlexButton(&showHidden_));
	fb.items.add(createFlexButton(&onlyUntagged_));
	fb.items.add(createFlexButton(&onlyDuplicates_));
	fb.items.add(createFlexButton(&andCategories_));
	fb.performLayout(normalFilter);
	auto flexBoxSize = FlexBoxHelper::computeFlexBoxSize(fb);
	auto favRow = normalFilter.removeFromTop((int) flexBoxSize.getHeight());

	auto filterRow = normalFilter; 
	auto catFilterMin = categoryFilters_.determineSubAreaForButtonLayout(this, filterRow);
	categoryFilters_.setBounds(catFilterMin.toNearestInt());

	int normalFilterHeight = (int) flexBoxSize.getHeight() + categoryFilters_.getHeight();

	auto sourceRow = leftHalf.removeFromTop(normalFilterHeight);
	textSearch_.setBounds(sourceRow.withSizeKeepingCentre(leftPart, LAYOUT_LARGE_LINE_HEIGHT));

	buttonDisplayType_.setBounds(sortAndDisplayTypeArea.removeFromTop(normalFilterHeight).withSizeKeepingCentre(LAYOUT_BUTTON_WIDTH, LAYOUT_BUTTON_HEIGHT));

	area.removeFromTop(normalFilterHeight);
	// Patch Buttons get the rest
	patchButtons_->setBounds(area.withTrimmedRight(LAYOUT_INSET_NORMAL).withTrimmedLeft(LAYOUT_INSET_NORMAL).withTrimmedTop(LAYOUT_INSET_NORMAL * 2));
}

void PatchSearchComponent::loadFilter(midikraft::PatchFilter filter) {
	// Set category buttons
	std::set<CategoryButtons::Category> activeCategories;
	for (auto const& c : filter.categories) {
		activeCategories.insert(CategoryButtons::Category(c.category(), c.color()));
	}
	categoryFilters_.setActive(activeCategories);

	// Set fav, show hidden button
	onlyFaves_.setToggleState(filter.onlyFaves, dontSendNotification);
	onlyUntagged_.setToggleState(filter.onlyUntagged, dontSendNotification);
	showHidden_.setToggleState(filter.showHidden, dontSendNotification);
	onlyDuplicates_.setToggleState(filter.onlyDuplicateNames, dontSendNotification);
	andCategories_.setToggleState(filter.andCategories, dontSendNotification);

	// Set name filter
	textSearch_.setSearchText(filter.name);
}

bool PatchSearchComponent::isInMultiSynthMode() {
	return !UIModel::currentSynth() || UIModel::instance()->multiMode_.multiSynthMode();
}

midikraft::PatchFilter PatchSearchComponent::getFilter() 
{	
	if (!isInMultiSynthMode()) {
		auto name = UIModel::currentSynth()->getName();
		if (synthSpecificFilter_.find(name) != synthSpecificFilter_.end()) {
			return synthSpecificFilter_[name];
		}
	}
	else {
		// Multi mode (or no synth) - user the stored multi mode filter
		multiModeFilter_.synths = allSynthsMap();
		return multiModeFilter_;
	}
	return buildFilter();
}

void PatchSearchComponent::updateCurrentFilter()
{
	if (!isInMultiSynthMode()) {
		auto name = UIModel::currentSynth()->getName();
		synthSpecificFilter_[name] = buildFilter();
	}
	else {
		multiModeFilter_ = buildFilter();
	}
}

midikraft::PatchFilter PatchSearchComponent::buildFilter() const
{
	// Transform into real category
	std::set<midikraft::Category> catSelected;
	for (auto c : categoryFilters_.selectedCategories()) {
		for (auto dc : database_.getCategories()) {
			if (dc.category() == c.category) {
				catSelected.emplace(dc);
				break;
			}
		}
	}
	bool typeSelected = false;
	int filterType = 0;
	/*if (advancedFilters_->dataTypeSelector_.getSelectedId() > 1) { // 0 is empty drop down, and 1 is "All data types"
		typeSelected = true;
		filterType = advancedFilters_->dataTypeSelector_.getSelectedId() - 2;
	}*/
	std::string nameFilter = "";
	if (!textSearch_.searchText().startsWith("!")) {
		nameFilter = textSearch_.searchText().toStdString();
	}
	std::map<std::string, std::weak_ptr<midikraft::Synth>> synthMap;
	// Build synth list - either the selected synth or all active synths
	if (isInMultiSynthMode()) {
		synthMap = allSynthsMap();
	}
	else {
		if (UIModel::currentSynth()) {
			synthMap[UIModel::currentSynth()->getName()] = UIModel::instance()->currentSynth_.smartSynth();
		}
	}
	return { synthMap,
		"", // Import filter is not controlled by the PatchSearchComponent anymore, but by the PatchView
		"", // List filter is not controlled by the PatchSearchComponent, but rather inserted by the PatchView who knows about the selection in the right hand tree view
		nameFilter,
		onlyFaves_.getToggleState(),
		typeSelected,
		filterType,
		showHidden_.getToggleState(),
		onlyUntagged_.getToggleState(),
		catSelected,
		andCategories_.getToggleState(),
		onlyDuplicates_.getToggleState()
	};
}

void PatchSearchComponent::changeListenerCallback(ChangeBroadcaster* source)
{
	if (dynamic_cast<CurrentSynth*>(source) || dynamic_cast<CurrentMultiMode*>(source)) {
		auto currentSynth = UIModel::instance()->currentSynth_.smartSynth();
        std::string synthName = "none";
        if (currentSynth) {
            synthName = currentSynth->getName();
        }
		categoryFilters_.setCategories(patchView_->predefinedCategories());

		// Set display type selected for this synth!
		auto synthNameForUI = currentSynthNameWithMulti();
		auto displayType = PatchHolderButton::getCurrentInfoForSynth(synthNameForUI);
		buttonDisplayType_.setSelectedId(0, dontSendNotification);
		for (int id = 0; id < kDisplayChoices.size(); id++) {
			if (kDisplayChoices[id].second == static_cast<int>(displayType)) {
				buttonDisplayType_.setSelectedId(id+1, dontSendNotification);
			}
		}

		// Rebuild the other features
		rebuildDataTypeFilterBox();

		if (isInMultiSynthMode()) {
			loadFilter(multiModeFilter_);
		}
		else {
			// Load the filter stored for this synth
			if (synthSpecificFilter_.find(synthName) == synthSpecificFilter_.end()) {
				// First time this synth is selected, store a new blank filter for this synth
				synthSpecificFilter_[synthName] = midikraft::PatchDatabase::allForSynth(currentSynth);
			}
			loadFilter(synthSpecificFilter_[synthName]);
		}

		patchView_->retrieveFirstPageFromDatabase();
		resized();
	}
	else if (dynamic_cast<CurrentSynthList*>(source)) {
		multiModeFilter_.synths = allSynthsMap();
		if (isInMultiSynthMode())
			patchView_->retrieveFirstPageFromDatabase();
	}
	else if (source == &UIModel::instance()->categoriesChanged) {
		categoryFilters_.setCategories(patchView_->predefinedCategories());
		patchView_->retrieveFirstPageFromDatabase();
		resized();
	}
}

void PatchSearchComponent::rebuildDataTypeFilterBox()
{
	/*advancedFilters_->dataTypeSelector_.clear();
	auto dflc = midikraft::Capability::hasCapability<midikraft::DataFileLoadCapability>(UIModel::instance()->currentSynth_.smartSynth());
	if (dflc) {
		StringArray typeNameList;
		typeNameList.add(kAllDataTypesFilter);
		for (size_t i = 0; i < dflc->dataTypeNames().size(); i++) {
			auto typeName = dflc->dataTypeNames()[i];
			if (typeName.canBeSent) {
				typeNameList.add(typeName.name);
			}
		}
		advancedFilters_->dataTypeSelector_.addItemList(typeNameList, 1);
	}*/
}

juce::String PatchSearchComponent::advancedTextSearch() const
{
	return textSearch_.searchText();
}

