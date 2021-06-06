/*
   Copyright (c) 2021 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "PatchSearchComponent.h"

#include "LayoutConstants.h"
#include "UIModel.h"

const char* kAllDataTypesFilter = "All types";

// A little helper
CategoryButtons::Category synthCategory(midikraft::NamedDeviceCapability* name) {
	return CategoryButtons::Category(name->getName(), Colours::black);
}

class AdvancedFilterPanel : public Component {
public:
	AdvancedFilterPanel(PatchView* patchView) :
		synthFilters_({}, [patchView](CategoryButtons::Category) { patchView->retrieveFirstPageFromDatabase();  }, false, true)
	{
		addAndMakeVisible(synthFilters_);
		addAndMakeVisible(dataTypeSelector_);
		dataTypeSelector_.setTextWhenNoChoicesAvailable("This synth does not support different data types");
		dataTypeSelector_.setTextWhenNothingSelected("Click here to show only data of a specific type");
		dataTypeSelector_.onChange = [patchView]() { patchView->retrieveFirstPageFromDatabase();  };
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
	categoryFilters_({}, [this](CategoryButtons::Category) { patchView_->retrieveFirstPageFromDatabase(); }, true, true),
	textSearch_([this]() { patchView_->retrieveFirstPageFromDatabase();  })

{
	advancedFilters_ = std::make_unique<AdvancedFilterPanel>(patchView_);

	textSearch_.setFontSize(LAYOUT_LARGE_FONT_SIZE);

	addAndMakeVisible(importList_); //TODO - this is actually no longer visible, but still used!?
	importList_.setTextWhenNoChoicesAvailable("No previous import data found");
	importList_.setTextWhenNothingSelected("Click here to filter for a specific import");
	importList_.onChange = [this]() { patchView_->retrieveFirstPageFromDatabase(); };

	onlyFaves_.setButtonText("Only Faves");
	onlyFaves_.onClick = [this]() { patchView_->retrieveFirstPageFromDatabase();  };
	addAndMakeVisible(onlyFaves_);

	showHidden_.setButtonText("Also Hidden");
	showHidden_.onClick = [this]() { patchView_->retrieveFirstPageFromDatabase();  };
	addAndMakeVisible(showHidden_);

	onlyUntagged_.setButtonText("Only Untagged");
	onlyUntagged_.onClick = [this]() { patchView_->retrieveFirstPageFromDatabase();  };
	addAndMakeVisible(onlyUntagged_);

	advancedSearch_ = std::make_unique<CollapsibleContainer>("Advanced filters", advancedFilters_.get(), false);
	addAndMakeVisible(*advancedSearch_);
	addAndMakeVisible(categoryFilters_);
	addAndMakeVisible(textSearch_);
	addAndMakeVisible(patchButtons_);

	UIModel::instance()->currentSynth_.addChangeListener(this);
	UIModel::instance()->synthList_.addChangeListener(this);
	UIModel::instance()->categoriesChanged.addChangeListener(this);
}

PatchSearchComponent::~PatchSearchComponent()
{
	UIModel::instance()->categoriesChanged.removeChangeListener(this);
	UIModel::instance()->currentSynth_.removeChangeListener(this);
	UIModel::instance()->synthList_.removeChangeListener(this);
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
	auto normalFilter = area.withTrimmedLeft(LAYOUT_INSET_NORMAL).withTrimmedRight(LAYOUT_INSET_NORMAL).withTrimmedTop(LAYOUT_INSET_NORMAL);
	auto leftHalf = normalFilter.removeFromLeft(leftPart);

	auto favRow = normalFilter.removeFromTop(LAYOUT_LINE_HEIGHT);
	FlexBox fb;
	fb.flexWrap = FlexBox::Wrap::wrap;
	fb.flexDirection = FlexBox::Direction::row;
	fb.justifyContent = FlexBox::JustifyContent::center;
	fb.items.add(createFlexButton(&onlyFaves_));
	fb.items.add(createFlexButton(&showHidden_));
	fb.items.add(createFlexButton(&onlyUntagged_));
	fb.performLayout(favRow);

	auto filterRow = normalFilter; 
	categoryFilters_.setBounds(filterRow.withTrimmedTop(LAYOUT_INSET_NORMAL));

	// Determine how much space out flex box needed!
	int usedHeight = categoryFilters_.usedHeight() + LAYOUT_LINE_SPACING;
	
	auto sourceRow = leftHalf.removeFromTop(usedHeight);
	textSearch_.setBounds(sourceRow.withSizeKeepingCentre(leftPart, LAYOUT_LARGE_LINE_HEIGHT));

	area.removeFromTop(usedHeight);
	advancedSearch_->setBounds(area.withTrimmedLeft(LAYOUT_INSET_NORMAL).withTrimmedRight(LAYOUT_INSET_NORMAL));

	// Patch Buttons get the rest
	int advancedFilterHeight = advancedSearch_->isOpen() ? advancedFilters_->synthFilters_.usedHeight() + LAYOUT_LARGE_LINE_HEIGHT : LAYOUT_LARGE_LINE_HEIGHT;
	area.removeFromTop(advancedFilterHeight);
	patchButtons_->setBounds(area.withTrimmedRight(LAYOUT_INSET_NORMAL).withTrimmedLeft(LAYOUT_INSET_NORMAL));
}

midikraft::PatchDatabase::PatchFilter PatchSearchComponent::buildFilter()
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
	if (advancedFilters_->dataTypeSelector_.getSelectedId() > 1) { // 0 is empty drop down, and 1 is "All data types"
		typeSelected = true;
		filterType = advancedFilters_->dataTypeSelector_.getSelectedId() - 2;
	}
	std::string nameFilter = "";
	if (!textSearch_.searchText().startsWith("!")) {
		nameFilter = textSearch_.searchText().toStdString();
	}
	std::map<std::string, std::weak_ptr<midikraft::Synth>> synthMap;
	// Build synth list
	for (auto s : advancedFilters_->synthFilters_.selectedCategories()) {
		midikraft::SynthHolder synthFound = UIModel::instance()->synthList_.synthByName(s.category);
		if (synthFound.synth()) {
			synthMap[synthFound.synth()->getName()] = synthFound.synth();
		}
	}
	return { synthMap,
		currentlySelectedSourceUUID(),
		nameFilter,
		onlyFaves_.getToggleState(),
		typeSelected,
		filterType,
		showHidden_.getToggleState(),
		onlyUntagged_.getToggleState(),
		catSelected };
}

void PatchSearchComponent::changeListenerCallback(ChangeBroadcaster* source)
{
	auto currentSynth = dynamic_cast<CurrentSynth*>(source);
	if (currentSynth) {
		categoryFilters_.setCategories(patchView_->predefinedCategories());

		// Select only the newly selected synth in the synth filters
		if (UIModel::currentSynth()) {
			advancedFilters_->synthFilters_.setActive({ synthCategory(UIModel::currentSynth()) });
		}

		// Rebuild the other features
		rebuildImportFilterBox();
		rebuildDataTypeFilterBox();
		patchView_->retrieveFirstPageFromDatabase();
		resized();
	}
	else if (dynamic_cast<CurrentSynthList*>(source)) {
		rebuildSynthFilters();
		resized();
	}
	else if (source == &UIModel::instance()->categoriesChanged) {
		categoryFilters_.setCategories(patchView_->predefinedCategories());
		patchView_->retrieveFirstPageFromDatabase();
		resized();
	}
}

void PatchSearchComponent::rebuildSynthFilters()
{
	// The available list of synths changed, reset the synth filters
	std::vector<CategoryButtons::Category> synthFilter;
	for (auto synth : UIModel::instance()->synthList_.activeSynths()) {
		synthFilter.push_back(synthCategory(synth.get()));
	}
	advancedFilters_->synthFilters_.setCategories(synthFilter);
	if (UIModel::currentSynth()) {
		advancedFilters_->synthFilters_.setActive({ synthCategory(UIModel::currentSynth()) });
	}
}

void PatchSearchComponent::rebuildImportFilterBox()
{
	importList_.clear();
	importList_.addItemList(patchView_->sourceNameList(), 1);
}

void PatchSearchComponent::rebuildDataTypeFilterBox()
{
	advancedFilters_->dataTypeSelector_.clear();
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
	}
}

void PatchSearchComponent::selectImportByID(String id)
{
	std::string description = "All patches"; // Hackish - if the ID is not found, select the All patches item. This needs to be fixed once we have a more general List type
	for (auto import : patchView_->imports_) {
		if (import.id == id) {
			description = import.description;
			break;
		}
	}
	for (int j = 0; j < importList_.getNumItems(); j++) {
		if (importList_.getItemText(j).toStdString() == description) {
			importList_.setSelectedItemIndex(j, sendNotificationAsync);
			break;
		}
	}
}

void PatchSearchComponent::selectImportByDescription(std::string const& description)
{
	// Search for the import in the sorted list
	for (int j = 0; j < importList_.getNumItems(); j++) {
		if (importList_.getItemText(j).toStdString() == description) {
			importList_.setSelectedItemIndex(j, dontSendNotification);
			break;
		}
	}
}

std::string PatchSearchComponent::currentlySelectedSourceUUID()
{
	if (importList_.getSelectedItemIndex() > 0) {
		std::string selectedItemText = importList_.getText().toStdString();
		for (auto import : patchView_->imports_) {
			if (import.description == selectedItemText) {
				return import.id;
			}
		}
		jassertfalse;
	}
	return "";
}

bool PatchSearchComponent::atLeastOneSynth()
{
	return !advancedFilters_->synthFilters_.selectedCategories().empty();
}

juce::String PatchSearchComponent::advancedTextSearch() const
{
	return textSearch_.searchText();
}

