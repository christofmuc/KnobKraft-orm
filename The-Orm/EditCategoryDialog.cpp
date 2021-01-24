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
		addAndMakeVisible(color);
		active.onClick = [a, this]() {
			a->value() = active.getToggleState();
		};
		setProps(a, n, c);
	}

	void setProps(TProp a, TProp n, TProp c) {
		active.setToggleState(a->value().getValue(), dontSendNotification);
		name.setText(n->value().getValue());
		//color.setCurrentColour(Colour::fromString(c->value().getValue().operator juce::String()));
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
		return ((int)props_.size()) / 3;
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
					existing->setProps(props_[rowNumber * 3], props_[rowNumber * 3 + 1], props_[rowNumber * 3 + 2]);
					return existing;
				}
			}
			return new CategoryRow(props_[rowNumber * 3], props_[rowNumber * 3 + 1], props_[rowNumber * 3 + 2]);
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

	std::string header = "Define categories";
	for (auto cat : cats) {
		props_.push_back(std::make_shared<TypedNamedValue>(TypedNamedValue("Active", header, true)));
		props_.push_back(std::make_shared<TypedNamedValue>(TypedNamedValue("Name", header, cat.category, 30)));
		props_.push_back(std::make_shared<TypedNamedValue>(TypedNamedValue("Color", header, cat.color)));
	}

	parameters_.setModel(new CategoryListModel(props_));
	//parameters_.setRowHeight(60);
	addAndMakeVisible(parameters_);

	ok_.onClick = [this]() { sWindow_->exitModalState(true);  };
	ok_.setButtonText("Save");
	addAndMakeVisible(ok_);

	cancel_.onClick = [this]() { sWindow_->exitModalState(false); };
	cancel_.setButtonText("Cancel");
	addAndMakeVisible(cancel_);

	// Finally we need a default size
	setBounds(0, 0, 540, 800);
}

void EditCategoryDialog::resized()
{
	auto area = getLocalBounds();
	auto buttonRow = area.removeFromBottom(40).withSizeKeepingCentre(220, 40);
	ok_.setBounds(buttonRow.removeFromLeft(100).reduced(4));
	cancel_.setBounds(buttonRow.removeFromLeft(100).reduced(4));
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

