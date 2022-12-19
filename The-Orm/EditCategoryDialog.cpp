/*
   Copyright (c) 2021 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "EditCategoryDialog.h"

#include "PropertyEditor.h"
#include "gin_gui/gin_gui.h"

static EditCategoryDialog::TCallback sCallback_;

class CategoryRow : public Component {
public:
	typedef std::shared_ptr<TypedNamedValue> TProp;
	CategoryRow(ValueTree catItem) : color(catItem.getPropertyAsValue("color", nullptr), "Color") {
		active.setClickingTogglesState(true);
		addAndMakeVisible(active);
		active.setEnabled(true);
		addAndMakeVisible(name);
		addAndMakeVisible(color);
		setRow(catItem);
	}

	virtual void resized() override {
		auto area = getLocalBounds();
		auto width = area.getWidth();
		active.setBounds(area.removeFromLeft(30));
		color.setBounds(area.removeFromRight(width * 30 / 100));
		name.setBounds(area.withTrimmedLeft(8).withTrimmedRight(8));
	}

	void setRow(ValueTree catItem) {
		// This changes the row to be displayed with this component (reusing components within a list box)
		name.getTextValue().referTo(catItem.getPropertyAsValue("name", nullptr));
		active.getToggleStateValue().referTo(catItem.getPropertyAsValue("active", nullptr));
		//color.getValueObject().referTo(catItem.getPropertyAsValue("color", nullptr));
	}

private:
	ToggleButton active;
	TextEditor name;
	gin::ColourPropertyComponent color;
};

class CategoryListModel: public ListBoxModel {
public:
	CategoryListModel(ValueTree categoryTree) : categoryTree_(categoryTree) {
	}

	int getNumRows() override
	{
		return categoryTree_.getNumChildren();
	}

	void paintListBoxItem(int rowNumber, Graphics& g, int width, int height, bool rowIsSelected) override
	{
		ignoreUnused(rowNumber, width, height, rowIsSelected);
		g.fillAll();
	}

	Component* refreshComponentForRow(int rowNumber, bool isRowSelected, Component* existingComponentToUpdate) override
	{
		ignoreUnused(isRowSelected);
		if (rowNumber < getNumRows()) {
			if (existingComponentToUpdate) {
				auto existing = dynamic_cast<CategoryRow *>(existingComponentToUpdate);
				if (existing) {
					existing->setRow(categoryTree_.getChild(rowNumber));
					return existing;
				}
				throw std::runtime_error("This was not the correct row type, can't continue");
			}
			return new CategoryRow(categoryTree_.getChild(rowNumber));
		}
		else {
			if (existingComponentToUpdate) {
				auto existing = dynamic_cast<CategoryRow*>(existingComponentToUpdate);
				if (existing) {
					existing->setRow(ValueTree("empty"));
					return existing;
				}
				throw std::runtime_error("This was not the correct row type, can't continue");
			}
			return nullptr;
		}
	}

private:
	ValueTree categoryTree_;
};


EditCategoryDialog::EditCategoryDialog() : propsTree_("categoryTree")
{	
	//parameters_.setRowHeight(60);
	parameters_ = std::make_unique<ListBox>();
	addAndMakeVisible(*parameters_);

	propsTree_.addListener(this);

	add_.onClick = [this]() {
		int nextID = nextId_++;
		addCategory({ nextID, true, "New category", Colour::fromString("191926")});
		//parameters_.setModel(new CategoryListModel(props_));
	};
	add_.setButtonText("Add new category");
	addAndMakeVisible(add_);

	ok_.onClick = []() { sWindow_->exitModalState(true);  };
	ok_.setButtonText("Save");
	addAndMakeVisible(ok_);

	cancel_.onClick = []() { sWindow_->exitModalState(false); };
	cancel_.setButtonText("Cancel");
	addAndMakeVisible(cancel_);

	// Finally we need a default size
	setBounds(0, 0, 540, 600);
}

EditCategoryDialog::~EditCategoryDialog()
{
	propsTree_.removeListener(this);
}

void EditCategoryDialog::refreshCategories(midikraft::PatchDatabase& db) {
	// Make a list of the categories we have...
	auto cats = db.getCategories();
	for (auto cat : cats) {
		addCategory(*cat.def());
	}
	parameters_->setModel(new CategoryListModel(propsTree_)); // To refresh
}

void EditCategoryDialog::refreshData() {
	parameters_->updateContent();
}

void EditCategoryDialog::resized()
{
	auto area = getLocalBounds();
	auto buttonRow = area.removeFromBottom(40).withSizeKeepingCentre(208, 40);
	ok_.setBounds(buttonRow.removeFromLeft(100).reduced(4));
	cancel_.setBounds(buttonRow.removeFromRight(100).reduced(4));
	auto addRow = area.removeFromBottom(80).withSizeKeepingCentre(208, 40);
	add_.setBounds(addRow);
	parameters_->setBounds(area.reduced(8));
}

static void dialogClosed(int modalResult, EditCategoryDialog* dialog)
{
	if (modalResult == 1 && dialog != nullptr) { // (must check that dialog isn't null in case it was deleted..)
		// Readout the properties and create a new list of Category definitions
		dialog->provideResult(sCallback_);
	}
	else {
		dialog->clearData();
	}
}

void EditCategoryDialog::showEditDialog(midikraft::PatchDatabase &db, Component *centeredAround, TCallback callback)
{
	if (!sEditCategoryDialog_) {
		sEditCategoryDialog_= std::make_unique<EditCategoryDialog>();
	}
	sCallback_ = callback;
	sEditCategoryDialog_->nextId_ = db.getNextBitindex(); // This is where we'll continue, but the user could press cancel using none of the newly made IDs
	sEditCategoryDialog_->refreshCategories(db);

	DialogWindow::LaunchOptions launcher;
	launcher.content.set(sEditCategoryDialog_.get(), false);
	launcher.componentToCentreAround = centeredAround;
	launcher.dialogTitle = "Edit categories";
	launcher.useNativeTitleBar = false;
	launcher.dialogBackgroundColour = Colours::black;
	sWindow_ = launcher.launchAsync();
	ModalComponentManager::getInstance()->attachCallback(sWindow_, ModalCallbackFunction::forComponent(dialogClosed, sEditCategoryDialog_.get()));

}

void EditCategoryDialog::provideResult(TCallback callback)
{
	std::vector<midikraft::CategoryDefinition> result;
	for (int i = 0; i < propsTree_.getNumChildren(); i++) {
		auto child = propsTree_.getChild(i);
		int id = child.getProperty("id");
		bool active = child.getProperty("active");
		String name = child.getProperty("name");
		Colour color = Colour::fromString(child.getProperty("color").toString());
		result.push_back({ id, active, name.toStdString(), color});
	}
	callback(result);
}

void EditCategoryDialog::clearData()
{
	propsTree_.removeAllChildren(nullptr);
}

void EditCategoryDialog::shutdown()
{
	sEditCategoryDialog_.reset();
}

void EditCategoryDialog::valueTreePropertyChanged(ValueTree& treeWhosePropertyHasChanged, const Identifier& property)
{
	ignoreUnused(treeWhosePropertyHasChanged, property);
	refreshData();
}

void EditCategoryDialog::valueTreeChildAdded(ValueTree& parentTree, ValueTree& childWhichHasBeenAdded)
{
	ignoreUnused(parentTree, childWhichHasBeenAdded);
	refreshData();
}

void EditCategoryDialog::valueTreeChildRemoved(ValueTree& parentTree, ValueTree& childWhichHasBeenRemoved, int indexFromWhichChildWasRemoved)
{
	ignoreUnused(parentTree, childWhichHasBeenRemoved, indexFromWhichChildWasRemoved);
	refreshData();
}

void EditCategoryDialog::addCategory(midikraft::CategoryDefinition const& def)
{
	// Search the tree for this category
	for (int i = 0; i < propsTree_.getNumChildren(); i++) {
		auto child = propsTree_.getChild(i);
		if (child.hasProperty("id") && def.id == ((int)child.getProperty("id"))) {
			// Found, update
			child.setProperty("name", String(def.name), nullptr);
			child.setProperty("active", def.isActive, nullptr);
			child.setProperty("color", def.color.toString(), nullptr);
			return;
		}
	}
	// Not found - add a new child
	ValueTree newCategory("Category" + String(def.id));
	newCategory.setProperty("id", def.id, nullptr);
	newCategory.setProperty("name", String(def.name), nullptr);
	newCategory.setProperty("active", def.isActive, nullptr);
	newCategory.setProperty("color", def.color.toString(), nullptr);
	propsTree_.addChild(newCategory, -1, nullptr);
}

std::unique_ptr<EditCategoryDialog> EditCategoryDialog::sEditCategoryDialog_;

juce::DialogWindow * EditCategoryDialog::sWindow_;

		