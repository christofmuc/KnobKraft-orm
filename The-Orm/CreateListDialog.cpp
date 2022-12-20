/*
   Copyright (c) 2021 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "CreateListDialog.h"

#include "LayoutConstants.h"
#include "SynthBank.h"

#include "Capability.h"
#include "HasBanksCapability.h"

std::map<int, std::string> bankLookup(std::shared_ptr<midikraft::Synth> synth)
{
	std::map<int, std::string> result;
	auto desc = midikraft::Capability::hasCapability<midikraft::HasBankDescriptorsCapability>(synth);
	if (desc) {
		int i = 0;
		for (auto const& d : desc->bankDescriptors())
		{
			if (!d.isROM) {
				result.insert({ i, d.name });
			}
			i++;
		}
	}
	else {
		auto banks = midikraft::Capability::hasCapability<midikraft::HasBanksCapability>(synth);
		if (banks) {
			for (int i = 0; i < banks->numberOfBanks(); i++)
			{
				result.insert({i, banks->friendlyBankName(MidiBankNumber::fromZeroBase(i, banks->numberOfPatches()))});
			}
		}
	}
	return result;
}

CreateListDialog::CreateListDialog(std::shared_ptr<midikraft::Synth> synth, TCallback &callback, TCallback &deleteCallback) : 
	synth_(synth)
	, isBank_(true)
	, callback_(callback)
	, deleteCallback_(deleteCallback)
{
	addAndMakeVisible(propertyEditor_);

	ok_.setButtonText("OK");
	ok_.addListener(this);
	addAndMakeVisible(ok_);

	cancel_.setButtonText("Cancel");
	cancel_.addListener(this);
	addAndMakeVisible(cancel_);

	delete_.setButtonText("Delete Bank");
	delete_.addListener(this);
	addAndMakeVisible(delete_);
	delete_.setVisible(false);

	// Finally we need a default size
	setBounds(0, 0, 540, 200);

	PropertyEditor::TProperties props;
	props.push_back(std::make_shared<TypedNamedValue>("Name", "General", "new list", -1));
	auto lookup = bankLookup(synth);
	props.push_back(std::make_shared<TypedNamedValue>("Bank", "General", 0, lookup));
	nameValue_ = Value(props[0]->value());
	jassert(nameValue_.refersToSameSourceAs(props[0]->value()));
	bankValue_ = Value(props[1]->value());
	propertyEditor_.setProperties(props);
}

CreateListDialog::CreateListDialog(TCallback& callback, TCallback& deleteCallback) : isBank_(false), callback_(callback), deleteCallback_(deleteCallback)
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
		nameValue_.setValue(isBank_ ? "new bank" : "new list");
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
	if (dialog) {
		dialog->release();
	}
}

void CreateListDialog::showCreateListDialog(std::shared_ptr<midikraft::SynthBank> list, std::shared_ptr<midikraft::Synth> synth, Component* centeredAround, TCallback callback, TCallback deleteCallback)
{
	if (!sCreateListDialog_) {
		sCreateListDialog_ = std::make_unique<CreateListDialog>(synth, callback, deleteCallback);
	}
	sCreateListDialog_->setList(list);
	sCreateListDialog_->callback_ = callback;
	sCreateListDialog_->deleteCallback_ = deleteCallback;

	DialogWindow::LaunchOptions launcher;
	launcher.content.set(sCreateListDialog_.get(), false);
	launcher.componentToCentreAround = centeredAround;
	launcher.dialogTitle = list ? "Edit user bank" : "Create user bank";
	launcher.useNativeTitleBar = false;
	launcher.dialogBackgroundColour = Colours::black;
	sWindow_ = launcher.launchAsync();
	ModalComponentManager::getInstance()->attachCallback(sWindow_, ModalCallbackFunction::forComponent(dialogClosed, sCreateListDialog_.get()));
}

void CreateListDialog::showCreateListDialog(std::shared_ptr<midikraft::PatchList> list, Component* centeredAround, TCallback callback, TCallback deleteCallback)
{
	if (!sCreateListDialog_) {
		sCreateListDialog_ = std::make_unique<CreateListDialog>(callback, deleteCallback);
	}
	sCreateListDialog_->setList(list);
	sCreateListDialog_->callback_ = callback;
	sCreateListDialog_->deleteCallback_ = deleteCallback;

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
		if (isBank_) {
			int bankSelected = bankValue_.getValue();
			bank_ = MidiBankNumber::fromZeroBase(bankSelected, midikraft::SynthBank::numberOfPatchesInBank(synth_, bankSelected));
			list_ = std::make_shared<midikraft::SynthBank>(name.toStdString(), synth_, bank_);
		}
		else {
			list_ = std::make_shared<midikraft::PatchList>(name.toStdString());
		}
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
