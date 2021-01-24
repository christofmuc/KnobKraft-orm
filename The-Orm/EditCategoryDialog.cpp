/*
   Copyright (c) 2021 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "EditCategoryDialog.h"

#include "PropertyEditor.h"
#include "gin/gin.h"

static std::function<void()> sCallback_;

class CategoryRow : public Component {
public:
	typedef std::shared_ptr<TypedNamedValue> TProp;
	CategoryRow(TProp a, TProp n, TProp c) : color(c->value(), "Color") {
		active.setClickingTogglesState(true);
		addAndMakeVisible(active);
		active.setEnabled(true);
		addAndMakeVisible(name);
		name.getTextValue().referTo(n->value());
		addAndMakeVisible(color);
		active.onClick = [a, this]() {
			a->value() = active.getToggleState();
		};
		setProps(a, n, c);
	}

	void setProps(TProp a, TProp n, TProp c) {
		active.setToggleState(a->value().getValue(), dontSendNotification);
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
	CategoryListModel(PropertyEditor::TProperties const &props) : props_(props) {
	}

	int getNumRows() override
	{
		return ((int)props_.size()) / 4;
	}

	void paintListBoxItem(int rowNumber, Graphics& g, int width, int height, bool rowIsSelected) override
	{
		ignoreUnused(rowNumber, g, width, height, rowIsSelected);
	}

	Component* refreshComponentForRow(int rowNumber, bool isRowSelected, Component* existingComponentToUpdate) override
	{
		ignoreUnused(isRowSelected, existingComponentToUpdate);
		if (rowNumber < getNumRows()) {
			if (existingComponentToUpdate) {
				auto existing = dynamic_cast<CategoryRow *>(existingComponentToUpdate);
				if (existing) {
					existing->setProps(props_[rowNumber * 4], props_[rowNumber * 4 + 1], props_[rowNumber * 4 + 2]);
					return existing;
				}
			}
			return new CategoryRow(props_[rowNumber * 4], props_[rowNumber * 4 + 1], props_[rowNumber * 4 + 2]);
		}
		else {
			return nullptr;
		}
	}

private:
	PropertyEditor::TProperties props_;
};


EditCategoryDialog::EditCategoryDialog(midikraft::PatchDatabase &database) 
{
	// Make a list of the categories we have...
	auto cats = database.getCategories();

	for (auto cat : cats) {
		addCategory(cat);
	}

	parameters_.setModel(new CategoryListModel(props_));
	//parameters_.setRowHeight(60);
	addAndMakeVisible(parameters_);

	add_.onClick = [this]() {
		int nextID = 1;
		addCategory({ nextID, true, "New category", Colours::aquamarine});
		parameters_.setModel(new CategoryListModel(props_));
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

void EditCategoryDialog::addCategory(midikraft::PatchDatabase::CategoryDefinition const &def) {
	std::string header = "Define categories";
	props_.push_back(std::make_shared<TypedNamedValue>(TypedNamedValue("Active", header, def.isActive)));
	props_.push_back(std::make_shared<TypedNamedValue>(TypedNamedValue("Name", header, def.name, 30)));
	props_.push_back(std::make_shared<TypedNamedValue>(TypedNamedValue("Color", header, def.color)));
	props_.push_back(std::make_shared<TypedNamedValue>(TypedNamedValue("ID", header, def.id)));
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
		sCallback_();
	}
}

void EditCategoryDialog::showEditDialog(midikraft::PatchDatabase &db, Component *centeredAround, std::function<void()> callback)
{
	if (!sEditCategoryDialog_) {
		sEditCategoryDialog_= std::make_unique<EditCategoryDialog>(db);
	}
	sCallback_ = callback;

	DialogWindow::LaunchOptions launcher;
	launcher.content.set(sEditCategoryDialog_.get(), false);
	launcher.componentToCentreAround = centeredAround;
	launcher.dialogTitle = "Edit categories";
	launcher.useNativeTitleBar = false;
	launcher.dialogBackgroundColour = Colours::black;
	sWindow_ = launcher.launchAsync();
	ModalComponentManager::getInstance()->attachCallback(sWindow_, ModalCallbackFunction::forComponent(dialogClosed, sEditCategoryDialog_.get()));

}

std::unique_ptr<EditCategoryDialog> EditCategoryDialog::sEditCategoryDialog_;

juce::DialogWindow * EditCategoryDialog::sWindow_;

