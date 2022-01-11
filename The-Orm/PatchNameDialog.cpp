/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/


#include "PatchNameDialog.h"

#include "Capability.h"
#include "LayeredPatchCapability.h"

#include "Logger.h"

std::unique_ptr<PatchNameDialog> PatchNameDialog::sPatchNameDialog_;
DialogWindow *PatchNameDialog::sWindow_ = nullptr;

PatchNameDialog::PatchNameDialog(std::function<void(std::shared_ptr<midikraft::PatchHolder> result) > callback) : callback_(callback), ok_("OK"), cancel_("Cancel")
{
	addAndMakeVisible(propertyEditor_);

	ok_.addListener(this);
	addAndMakeVisible(ok_);

	cancel_.addListener(this);
	addAndMakeVisible(cancel_);

	// Finally we need a default size
	setBounds(0, 0, 540, 200);
}

void PatchNameDialog::setPatch(std::shared_ptr<midikraft::PatchHolder> patch)
{
	patch_ = patch;

	PropertyEditor::TProperties props;

	// Check if the patch is a layered patch
	auto layers = midikraft::Capability::hasCapability<midikraft::LayeredPatchCapability>(patch->patch());
	names_.clear();
	if (layers) {
		for (int i = 0; i < layers->numberOfLayers(); i++) {
			TypedNamedValue v("Layer " + String(i), "Patch name",String(layers->layerName(i)).trim(), 20);
			names_.push_back(v.value());
			props.push_back(std::make_shared<TypedNamedValue>(v));
		}
	}
	else if (patch->patch()) {
		TypedNamedValue v("Patch name", "Patch name", String(patch->name()).trim(), 20);
		names_.push_back(v.value());
		props.push_back(std::make_shared<TypedNamedValue>(v));
	}
	propertyEditor_.setProperties(props);
}

void PatchNameDialog::resized()
{
	auto area = getLocalBounds();
	auto buttonRow = area.removeFromBottom(40).withSizeKeepingCentre(220, 40);
	ok_.setBounds(buttonRow.removeFromLeft(100).reduced(4));
	cancel_.setBounds(buttonRow.removeFromLeft(100).reduced(4));
	propertyEditor_.setBounds(area);
}

static void dialogClosed(int modalResult, PatchNameDialog* dialog)
{
	if (modalResult == 1 && dialog != nullptr) { // (must check that dialog isn't null in case it was deleted..)
		dialog->notifyResult();
	}
}

void PatchNameDialog::showPatchNameDialog(std::shared_ptr<midikraft::PatchHolder> patch, Component *centeredAround, std::function<void(std::shared_ptr<midikraft::PatchHolder>)> callback)
{
	if (!patch || !patch->patch()) {
		return;
	}
	if (!sPatchNameDialog_) {
		sPatchNameDialog_ = std::make_unique<PatchNameDialog>(callback);
	}
	sPatchNameDialog_->setPatch(patch);

	DialogWindow::LaunchOptions launcher;
	launcher.content.set(sPatchNameDialog_.get(), false);
	launcher.componentToCentreAround = centeredAround;
	launcher.dialogTitle = "Edit Patch Name";
	launcher.useNativeTitleBar = false;
	launcher.dialogBackgroundColour = Colours::black;
	sWindow_ = launcher.launchAsync();
	ModalComponentManager::getInstance()->attachCallback(sWindow_, ModalCallbackFunction::forComponent(dialogClosed, sPatchNameDialog_.get()));
}

void PatchNameDialog::release()
{
	sPatchNameDialog_.reset();
}

void PatchNameDialog::notifyResult()
{
	callback_(patch_);
}

void PatchNameDialog::buttonClicked(Button *button)
{
	if (button == &ok_) {
		
		auto layers = midikraft::Capability::hasCapability<midikraft::LayeredPatchCapability>(patch_->patch());
		if (layers) {
			for (int i = 0; i < layers->numberOfLayers(); i++) {
				String newName = names_[i].getValue().toString();
				//SimpleLogger::instance()->postMessage("Layer " + String(i) + " is " + names_[i].getValue());
				layers->setLayerName(i, newName.toStdString());
			}
			patch_->setName(patch_->synth()->nameForPatch(patch_->patch()));
		}
		else {
			String newName = names_[0].getValue().toString();
			SimpleLogger::instance()->postMessage("Changed patch name to " + newName.toStdString());
			patch_->setName(newName.toStdString());
		}
		sWindow_->exitModalState(true);
	}
	else if (button == &cancel_) {
		sWindow_->exitModalState(false);
	}
}
