/*
   Copyright (c) 2021 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "CreateListDialog.h"

#include "LayoutConstants.h"

CreateListDialog::CreateListDialog(TCallback &callback, TCallback &deleteCallback) : callback_(callback), deleteCallback_(deleteCallback)
{
	addAndMakeVisible(propertyEditor_);

	ok_.setButtonText("OK");
	ok_.addListener(this);
	addAndMakeVisible(ok_);

	cancel_.setButtonText("Cancel");
	cancel_.addListener(this);
	addAndMakeVisible(cancel_);

	delete_.setButtonText("Delete List");
	delete_.addListener(this);
	addAndMakeVisible(delete_);
	delete_.setVisible(false);

	// Finally we need a default size
	setBounds(0, 0, 540, 200);

	PropertyEditor::TProperties props;
	props.push_back(std::make_shared<TypedNamedValue>("Name", "General", "new list", -1));
	nameValue_ = Value(props[0]->value());
	jassert(nameValue_.refersToSameSourceAs(props[0]->value()));
	propertyEditor_.setProperties(props);
}

void CreateListDialog::setList(std::shared_ptr<midikraft::PatchList> list)
{
	list_ = list;
	if (list_) {
		nameValue_.setValue(String(list->name()));
		delete_.setVisible(true);
	}
	else {
		nameValue_.setValue("new list");
		delete_.setVisible(false);
	}
}

void CreateListDialog::resized()
{
	auto area = getLocalBounds().reduced(LAYOUT_INSET_NORMAL);
	auto bottomRow = area.removeFromBottom(LAYOUT_LINE_SPACING);
	auto buttonRow = bottomRow.withSizeKeepingCentre(2 * LAYOUT_BUTTON_WIDTH + LAYOUT_INSET_NORMAL, LAYOUT_LINE_SPACING);
	ok_.setBounds(buttonRow.removeFromLeft(LAYOUT_BUTTON_WIDTH).reduced(LAYOUT_INSET_SMALL));
	cancel_.setBounds(buttonRow.removeFromLeft(LAYOUT_BUTTON_WIDTH).reduced(LAYOUT_INSET_SMALL));
	delete_.setBounds(area.removeFromBottom(2*LAYOUT_LINE_SPACING + LAYOUT_INSET_NORMAL).withSizeKeepingCentre(LAYOUT_BUTTON_WIDTH, LAYOUT_LINE_HEIGHT));
	propertyEditor_.setBounds(area);
}

static void dialogClosed(int modalResult, CreateListDialog* dialog)
{
	if (modalResult == 1 && dialog != nullptr) { // (must check that dialog isn't null in case it was deleted..)
		dialog->notifyResult();
	}
}

void CreateListDialog::showCreateListDialog(std::shared_ptr<midikraft::PatchList> list, Component* centeredAround, TCallback callback, TCallback deleteCallback)
{
	if (!sCreateListDialog_) {
		sCreateListDialog_ = std::make_unique<CreateListDialog>(callback, deleteCallback);
	}
	sCreateListDialog_->setList(list);

	DialogWindow::LaunchOptions launcher;
	launcher.content.set(sCreateListDialog_.get(), false);
	launcher.componentToCentreAround = centeredAround;
	launcher.dialogTitle = list ? "Edit user list" : "Create user list";
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
	if (list_) {
		list_->setName(name.toStdString());
	}
	else {
		// This was create mode!
		list_ = std::make_shared<midikraft::PatchList>(name.toStdString());
	}
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
	else if (button == &delete_) {
		if (list_ && AlertWindow::showOkCancelBox(AlertWindow::QuestionIcon, "Confirm deletion", "Do you really want to delete the list" + list_->name() + 
			"?\n\nThis will leave all patches in the database, but delete the list definition.\n",
			"Yes", "No, take me back!")) {
			deleteCallback_(list_);
			sWindow_->exitModalState(false);
		}
	}
}

std::unique_ptr<CreateListDialog> CreateListDialog::sCreateListDialog_;

juce::DialogWindow* CreateListDialog::sWindow_;
