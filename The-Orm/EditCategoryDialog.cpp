/*
   Copyright (c) 2021 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "EditCategoryDialog.h"

#include "PropertyEditor.h"
#include "gin_gui/gin_gui.h"

#include "LayoutConstants.h"

static EditCategoryDialog::TCallback sCallback_;

class CategoryRow : public Component {
public:
	typedef std::shared_ptr<TypedNamedValue> TProp;
	CategoryRow(ValueTree catItem) : color(catItem.getPropertyAsValue("color", nullptr), "") {
		active.setClickingTogglesState(true);
		addAndMakeVisible(active);
		active.setEnabled(true);
		addAndMakeVisible(name);
		addAndMakeVisible(order_num);
		addAndMakeVisible(color);
		setRow(catItem);
	}

	virtual void resized() override {
		auto area = getLocalBounds();
		active.setBounds(area.removeFromLeft(LAYOUT_BUTTON_WIDTH_MIN));
		color.setBounds(area.removeFromRight(LAYOUT_BUTTON_WIDTH));
		order_num.setBounds(area.removeFromRight(LAYOUT_BUTTON_WIDTH_MIN));
		name.setBounds(area.withTrimmedLeft(LAYOUT_INSET_NORMAL).withTrimmedRight(LAYOUT_INSET_NORMAL));
	}

	void setRow(ValueTree catItem) {
		// This changes the row to be displayed with this component (reusing components within a list box)
		name.getTextValue().referTo(catItem.getPropertyAsValue("name", nullptr));
		order_num.getTextValue().referTo(catItem.getPropertyAsValue("order_num", nullptr));
		active.getToggleStateValue().referTo(catItem.getPropertyAsValue("active", nullptr));
		color.getValueObject().referTo(catItem.getPropertyAsValue("color", nullptr));
	}

private:
	ToggleButton active;
	TextEditor name;
	TextEditor order_num;
	gin::ColourPropertyComponent color;
};

class TitleRow: public Component {
public:
	typedef std::shared_ptr<TypedNamedValue> TProp;
	TitleRow() {
		active.setText("Active", dontSendNotification);
		name.setText("Header", dontSendNotification);
		order_num.setText("Order", dontSendNotification);
		color.setText("Color", dontSendNotification);
		color.setJustificationType(juce::Justification(Justification::centred));
		addAndMakeVisible(active);
		addAndMakeVisible(name);
		addAndMakeVisible(order_num);
		addAndMakeVisible(color);
	}

	virtual void resized() override {
		auto area = getLocalBounds();
		active.setBounds(area.removeFromLeft(LAYOUT_BUTTON_WIDTH_MIN));
		color.setBounds(area.removeFromRight(LAYOUT_BUTTON_WIDTH));
		order_num.setBounds(area.removeFromRight(LAYOUT_BUTTON_WIDTH_MIN));
		name.setBounds(area.withTrimmedLeft(LAYOUT_INSET_NORMAL).withTrimmedRight(LAYOUT_INSET_NORMAL));
	}

private:
	Label active;
	Label name;
	Label order_num;
	Label color;
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
	tableHeader_ = std::make_unique<TitleRow>();
	addAndMakeVisible(tableHeader_.get());
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
	std::sort(cats.begin(), cats.end(), [](const midikraft::Category& a, const midikraft::Category& b) { return a.def()->id< b.def()->id;  });
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
	auto buttonRow = area.removeFromBottom(LAYOUT_LARGE_LINE_SPACING).withSizeKeepingCentre((LAYOUT_BUTTON_WIDTH+ LAYOUT_INSET_SMALL)*2, LAYOUT_LARGE_LINE_SPACING);
	ok_.setBounds(buttonRow.removeFromLeft(LAYOUT_BUTTON_WIDTH).reduced(LAYOUT_INSET_SMALL));
	cancel_.setBounds(buttonRow.removeFromRight(LAYOUT_BUTTON_WIDTH).reduced(LAYOUT_INSET_SMALL));
	auto addRow = area.removeFromBottom(LAYOUT_LARGE_LINE_SPACING * 2).withSizeKeepingCentre((LAYOUT_BUTTON_WIDTH + LAYOUT_INSET_SMALL) * 2, LAYOUT_LARGE_LINE_SPACING);
	add_.setBounds(addRow);
	tableHeader_->setBounds(area.removeFromTop(LAYOUT_LARGE_LINE_SPACING));
	parameters_->setBounds(area.reduced(LAYOUT_INSET_NORMAL));
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
		String order_num = child.getProperty("order_num");
		result.push_back({ id, active, name.toStdString(), color, order_num.getIntValue() });
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
			child.setProperty("order_num", def.sort_order, nullptr);
			return;
		}
	}
	// Not found - add a new child
	ValueTree newCategory("Category" + String(def.id));
	newCategory.setProperty("id", def.id, nullptr);
	newCategory.setProperty("name", String(def.name), nullptr);
	newCategory.setProperty("active", def.isActive, nullptr);
	newCategory.setProperty("color", def.color.toString(), nullptr);
	newCategory.setProperty("order_num", def.sort_order, nullptr);
	propsTree_.addChild(newCategory, -1, nullptr);
}

std::unique_ptr<EditCategoryDialog> EditCategoryDialog::sEditCategoryDialog_;

juce::DialogWindow * EditCategoryDialog::sWindow_;

		