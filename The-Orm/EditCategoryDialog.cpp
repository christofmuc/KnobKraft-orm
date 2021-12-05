/*
   Copyright (c) 2021 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "EditCategoryDialog.h"

#include "PropertyEditor.h"
#include "gin/gin.h"

static EditCategoryDialog::TCallback sCallback_;

class CategoryRow : public Component {
public:
	typedef std::shared_ptr<TypedNamedValue> TProp;
	CategoryRow(ValueTree catItem) : color(catItem.getPropertyAsValue("color", nullptr), "Color") {
		name.getTextValue().referTo(catItem.getPropertyAsValue("name", nullptr));
		active.getToggleStateValue().referTo(catItem.getPropertyAsValue("active", nullptr));
		active.setClickingTogglesState(true);
		addAndMakeVisible(active);
		active.setEnabled(true);
		addAndMakeVisible(name);
		addAndMakeVisible(color);
	}

	virtual void resized() override {
		auto area = getLocalBounds();
		active.setBounds(area.removeFromLeft(30));
		color.setBounds(area.removeFromRight(200));
		name.setBounds(area.withTrimmedLeft(8).withTrimmedRight(8));
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
		ignoreUnused(rowNumber, g, width, height, rowIsSelected);
	}

	Component* refreshComponentForRow(int rowNumber, bool isRowSelected, Component* existingComponentToUpdate) override
	{
		ignoreUnused(isRowSelected, existingComponentToUpdate);
		if (rowNumber < getNumRows()) {
			/*if (existingComponentToUpdate) {
				auto existing = dynamic_cast<CategoryRow *>(existingComponentToUpdate);
				if (existing) {
					existing->setProps(props_[rowNumber * 4], props_[rowNumber * 4 + 1], props_[rowNumber * 4 + 2]);
					return existing;
				}
			}*/
			//delete existingComponentToUpdate;
			return new CategoryRow(categoryTree_.getChild(rowNumber));
		}
		else {
			return nullptr;
		}
	}

private:
	ValueTree categoryTree_;
};


EditCategoryDialog::EditCategoryDialog(midikraft::PatchDatabase &database) : propsTree_("categoryTree")
{	
	//parameters_.setRowHeight(60);
	addAndMakeVisible(parameters_);

	add_.onClick = [this, &database]() {
		int nextID = nextId_++;
		addCategory({ nextID, true, "New category", Colours::aquamarine});
		//parameters_.setModel(new CategoryListModel(props_));
	};
	add_.setButtonText("Add new category");
	addAndMakeVisible(add_);

	ok_.onClick = [this]() { sWindow_->exitModalState(true);  };
	ok_.setButtonText("Save");
	addAndMakeVisible(ok_);

	cancel_.onClick = [this]() { sWindow_->exitModalState(false); };
	cancel_.setButtonText("Cancel");
	addAndMakeVisible(cancel_);

	// Finally we need a default size
	setBounds(0, 0, 540, 600);
}

void EditCategoryDialog::refreshCategories(midikraft::PatchDatabase& db) {
	// Make a list of the categories we have...
	auto cats = db.getCategories();
	for (auto cat : cats) {
		addCategory(*cat.def());
	}
	parameters_.setModel(new CategoryListModel(propsTree_)); // To refresh
}

void EditCategoryDialog::resized()
{
	auto area = getLocalBounds();
	auto buttonRow = area.removeFromBottom(40).withSizeKeepingCentre(208, 40);
	ok_.setBounds(buttonRow.removeFromLeft(100).reduced(4));
	cancel_.setBounds(buttonRow.removeFromRight(100).reduced(4));
	auto addRow = area.removeFromBottom(80).withSizeKeepingCentre(208, 40);
	add_.setBounds(addRow);
	parameters_.setBounds(area.reduced(8));
}

static void dialogClosed(int modalResult, EditCategoryDialog* dialog)
{
	if (modalResult == 1 && dialog != nullptr) { // (must check that dialog isn't null in case it was deleted..)
		// Readout the properties and create a new list of Category definitions
		dialog->provideResult(sCallback_);
	}
}

void EditCategoryDialog::showEditDialog(midikraft::PatchDatabase &db, Component *centeredAround, TCallback callback)
{
	if (!sEditCategoryDialog_) {
		sEditCategoryDialog_= std::make_unique<EditCategoryDialog>(db);
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

void EditCategoryDialog::shutdown()
{
	sEditCategoryDialog_.reset();
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

		