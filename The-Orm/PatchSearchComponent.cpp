/*
   Copyright (c) 2021 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "PatchSearchComponent.h"

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
		dataTypeSelector_.setBounds(area.removeFromLeft(200).withTrimmedRight(16));
		synthFilters_.setBounds(area);
	}

	ComboBox dataTypeSelector_;
	CategoryButtons synthFilters_;
};


PatchSearchComponent::PatchSearchComponent(PatchView* patchView, PatchButtonPanel* patchButtons, midikraft::PatchDatabase& database) :
	patchView_(patchView),
	patchButtons_(patchButtons),
	database_(database),
	categoryFilters_({}, [this](CategoryButtons::Category) { patchView_->retrieveFirstPageFromDatabase(); }, true, true)
{
	advancedFilters_ = std::make_unique<AdvancedFilterPanel>(patchView_);

	addAndMakeVisible(importList_);
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

	addAndMakeVisible(nameSearchText_);
	nameSearchText_.onTextChange = [this, patchView]() {
		if (nameSearchText_.getText().isNotEmpty()) {
			useNameSearch_.setToggleState(true, dontSendNotification);
		}
		patchView->retrieveFirstPageFromDatabase();
	};
	nameSearchText_.onEscapeKey = [this]() {
		nameSearchText_.setText("", true);
		useNameSearch_.setToggleState(false, dontSendNotification);
	};
	addAndMakeVisible(useNameSearch_);
	useNameSearch_.setButtonText("search in name");
	useNameSearch_.onClick = [patchView]() { patchView->retrieveFirstPageFromDatabase(); };


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

void PatchSearchComponent::resized()
{
	Rectangle<int> area(getLocalBounds());
	auto normalFilter = area.removeFromTop(32 * 2 + 24 + 3 * 8).reduced(8);
	auto sourceRow = normalFilter.removeFromTop(24);
	auto filterRow = normalFilter.withTrimmedTop(8); // 32 per row
	int advancedFilterHeight = advancedSearch_->isOpen() ? (24 + 24 + 2 * 32) : 24;
	advancedSearch_->setBounds(area.removeFromTop(advancedFilterHeight).withTrimmedLeft(8).withTrimmedRight(8));
	onlyUntagged_.setBounds(sourceRow.removeFromRight(100));
	showHidden_.setBounds(sourceRow.removeFromRight(100));
	onlyFaves_.setBounds(sourceRow.removeFromRight(100));
	categoryFilters_.setBounds(filterRow);
	nameSearchText_.setBounds(sourceRow.removeFromLeft(300));
	useNameSearch_.setBounds(sourceRow.removeFromLeft(100).withTrimmedLeft(8));
	importList_.setBounds(sourceRow.withTrimmedLeft(8));
	patchButtons_->setBounds(area.withTrimmedRight(8).withTrimmedLeft(8));
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
	if (useNameSearch_.getToggleState()) {
		if (!nameSearchText_.getText().startsWith("!")) {
			nameFilter = nameSearchText_.getText().toStdString();
		}
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
	}
	else if (dynamic_cast<CurrentSynthList*>(source)) {
		rebuildSynthFilters();
	}
	else if (source == &UIModel::instance()->categoriesChanged) {
		categoryFilters_.setCategories(patchView_->predefinedCategories());
		patchView_->retrieveFirstPageFromDatabase();
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
	std::string description;
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
	return nameSearchText_.getText();
}

