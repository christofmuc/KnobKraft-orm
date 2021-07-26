/*
   Copyright (c) 2021 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "CreateListDialog.h"


CreateListDialog::CreateListDialog(std::shared_ptr<midikraft::PatchList> list, TCallback &callback) : list_(list), callback_(callback)
{
	addAndMakeVisible(propertyEditor_);

	ok_.setButtonText("OK");
	ok_.addListener(this);
	addAndMakeVisible(ok_);

	cancel_.setButtonText("Cancel");
	cancel_.addListener(this);
	addAndMakeVisible(cancel_);

	// Finally we need a default size
	setBounds(0, 0, 540, 200);

	PropertyEditor::TProperties props;
	props.push_back(std::make_shared<TypedNamedValue>("Name", "General", "new list", -1));
	nameValue_.referTo(props[0]->value());
	propertyEditor_.setProperties(props);
}

void CreateListDialog::resized()
{
	auto area = getLocalBounds();
	auto buttonRow = area.removeFromBottom(40).withSizeKeepingCentre(220, 40);
	ok_.setBounds(buttonRow.removeFromLeft(100).reduced(4));
	cancel_.setBounds(buttonRow.removeFromLeft(100).reduced(4));
	propertyEditor_.setBounds(area);
}

static void dialogClosed(int modalResult, CreateListDialog* dialog)
{
	if (modalResult == 1 && dialog != nullptr) { // (must check that dialog isn't null in case it was deleted..)
		dialog->notifyResult();
	}
}

void CreateListDialog::showCreateListDialog(std::shared_ptr<midikraft::PatchList> list, Component* centeredAround, TCallback callback)
{
	if (!sCreateListDialog_) {
		sCreateListDialog_ = std::make_unique<CreateListDialog>(list, callback);
	}

	DialogWindow::LaunchOptions launcher;
	launcher.content.set(sCreateListDialog_.get(), false);
	launcher.componentToCentreAround = centeredAround;
	launcher.dialogTitle = "Create user list";
	launcher.useNativeTitleBar = false;
	launcher.dialogBackgroundColour = Colours::black;
	sWindow_ = launcher.launchAsync();
	ModalComponentManager::getInstance()->attachCallback(sWindow_, ModalCallbackFunction::forComponent(dialogClosed, sCreateListDialog_.get()));
}

void CreateListDialog::release()
{
	sCreateListDialog_.reset();
}

void CreateListDialog::notifyResult()
{
	String name = nameValue_.getValue();
	list_->setName(name.toStdString());
	callback_(list_);
}

void CreateListDialog::buttonClicked(Button* button) {
	if (button == &ok_) {
		//String newName = names_[0].getValue().toString();
		sWindow_->exitModalState(true);
	}
	else if (button == &cancel_) {
		sWindow_->exitModalState(false);
	}
}

std::unique_ptr<CreateListDialog> CreateListDialog::sCreateListDialog_;

juce::DialogWindow* CreateListDialog::sWindow_;
