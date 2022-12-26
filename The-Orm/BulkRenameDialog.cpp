/*
   Copyright (c) 2021 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "BulkRenameDialog.h"

#include "LayoutConstants.h"

#include "Capability.h"

BulkRenameDialog::BulkRenameDialog()
{
	addAndMakeVisible(propertyEditor_);

	ok_.setButtonText("OK");
	ok_.addListener(this);
	addAndMakeVisible(ok_);

	cancel_.setButtonText("Cancel");
	cancel_.addListener(this);
	addAndMakeVisible(cancel_);

	paste_.setButtonText("Paste");
	paste_.addListener(this);
	addAndMakeVisible(paste_);

	// Finally we need a default size
	setBounds(0, 0, 540, 600);
}

void BulkRenameDialog::setList(std::vector<midikraft::PatchHolder> input)
{
	input_ = input;
	propertyEditor_.clear();
	props_.clear();
	for (auto const& patch: input_) {
		props_.push_back(std::make_shared<TypedNamedValue>(patch.name(), "Names", patch.name(), 50));
	}
	propertyEditor_.setProperties(props_);
}

void BulkRenameDialog::resized()
{
	auto area = getLocalBounds().reduced(LAYOUT_INSET_NORMAL);
	auto bottomRow = area.removeFromBottom(LAYOUT_LINE_SPACING);
	auto buttonRow = bottomRow.withSizeKeepingCentre(2 * LAYOUT_BUTTON_WIDTH + LAYOUT_INSET_NORMAL, LAYOUT_LINE_SPACING);
	ok_.setBounds(buttonRow.removeFromLeft(LAYOUT_BUTTON_WIDTH).reduced(LAYOUT_INSET_SMALL));
	cancel_.setBounds(buttonRow.removeFromLeft(LAYOUT_BUTTON_WIDTH).reduced(LAYOUT_INSET_SMALL));
	paste_.setBounds(area.removeFromBottom(2*LAYOUT_LINE_SPACING + LAYOUT_INSET_NORMAL).withSizeKeepingCentre(LAYOUT_BUTTON_WIDTH, LAYOUT_LINE_HEIGHT));
	propertyEditor_.setBounds(area);
}

static void dialogClosed(int modalResult, BulkRenameDialog* dialog)
{
	if (modalResult == 1 && dialog != nullptr) { // (must check that dialog isn't null in case it was deleted..)
		dialog->notifyResult();
	}
	if (dialog) {
		dialog->release();
	}
}

void BulkRenameDialog::show(std::vector<midikraft::PatchHolder> input, Component* centeredAround, TCallback callback)
{
	if (!sBulkRenameDialog_) {
		sBulkRenameDialog_ = std::make_unique<BulkRenameDialog>();
	}
	sBulkRenameDialog_->setList(input);
	sBulkRenameDialog_->callback_ = callback;

	DialogWindow::LaunchOptions launcher;
	launcher.content.set(sBulkRenameDialog_.get(), false);
	launcher.componentToCentreAround = centeredAround;
	launcher.dialogTitle = "Bulk rename patches";
	launcher.useNativeTitleBar = false;
	launcher.dialogBackgroundColour = Colours::black;
	sWindow_ = launcher.launchAsync();
	ModalComponentManager::getInstance()->attachCallback(sWindow_, ModalCallbackFunction::forComponent(dialogClosed, sBulkRenameDialog_.get()));
}

void BulkRenameDialog::release()
{
	sBulkRenameDialog_.reset();
}

void BulkRenameDialog::notifyResult()
{
	// Copy out new values into input list names
	for (int i = 0; i < juce::jmin(input_.size(), props_.size()); i++) {
		input_[i].setName(props_[i]->value().toString().toStdString());
	}
	callback_(input_);
}

void BulkRenameDialog::buttonClicked(Button* button) {
	if (button == &ok_) {
		//String newName = names_[0].getValue().toString();
		sWindow_->exitModalState(true);
	}
	else if (button == &cancel_) {
		sWindow_->exitModalState(false);
	}
	else if (button == &paste_) {
		juce::StringArray tokens;
		tokens.addLines(juce::SystemClipboard::getTextFromClipboard());
		for (int i = 0; i < juce::jmin(tokens.size(), (int) props_.size()); i++) {
			props_[i]->value() = tokens[i];
		}
	}
}

std::unique_ptr<BulkRenameDialog> BulkRenameDialog::sBulkRenameDialog_;

juce::DialogWindow* BulkRenameDialog::sWindow_;
