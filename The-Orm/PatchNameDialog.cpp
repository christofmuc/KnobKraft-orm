/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/


#include "PatchNameDialog.h"

#include "LayeredPatch.h"

#include "Logger.h"

std::unique_ptr<PatchNameDialog> PatchNameDialog::sPatchNameDialog_;
std::unique_ptr<DialogWindow> PatchNameDialog::sWindow_;

PatchNameDialog::PatchNameDialog() : ok_("OK"), cancel_("Cancel")
{
	addAndMakeVisible(propertyEditor_);

	ok_.addListener(this);
	addAndMakeVisible(ok_);

	cancel_.addListener(this);
	addAndMakeVisible(cancel_);

	// Finally we need a default size
	setBounds(0, 0, 540, 200);
}

void PatchNameDialog::setPatch(midikraft::PatchHolder *patch)
{
	patch_ = patch;

	PropertyEditor::TProperties props;

	// Check if the patch is a layered patch
	auto layers = std::dynamic_pointer_cast<midikraft::LayeredPatch>(patch->patch()); 
	names_.clear();
	if (layers) {
		for (int i = 0; i < layers->numberOfLayers(); i++) {
			names_.push_back(Value(String(layers->layerName(i))));
			TypedNamedValue v({ "Layer " + String(i), "Patch name", names_[i], ValueType::String, 0, 20, {}, true});
			props.push_back(std::make_shared<TypedNamedValue>(v));
		}
	}
	else if (patch->patch()) {
		names_.push_back(Value(String(patch->patch()->patchName())));
		TypedNamedValue v({ "Patch name", "Patch name", names_[0], ValueType::String, 0, 20, {}, true });
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

void PatchNameDialog::showPatchNameDialog(midikraft::PatchHolder  *patch, Component *centeredAround)
{
	if (!patch || !patch->patch()) {
		return;
	}
	if (!sPatchNameDialog_) {
		sPatchNameDialog_ = std::make_unique<PatchNameDialog>();
	}
	sPatchNameDialog_->setPatch(patch);

	DialogWindow::LaunchOptions launcher;
	launcher.content.set(sPatchNameDialog_.get(), false);
	launcher.componentToCentreAround = centeredAround;
	launcher.dialogTitle = "Edit Patch Name";
	launcher.useNativeTitleBar = false;
	launcher.dialogBackgroundColour = Colours::black;
	sWindow_.reset(launcher.launchAsync());
}

void PatchNameDialog::release()
{
	sWindow_.reset();
	sPatchNameDialog_.reset();
}

void PatchNameDialog::buttonClicked(Button *button)
{
	if (button == &ok_) {
		auto layers = std::dynamic_pointer_cast<midikraft::LayeredPatch>(patch_->patch());
		if (layers) {
			for (int i = 0; i < layers->numberOfLayers(); i++) {
				SimpleLogger::instance()->postMessage("Layer " + String(i) + " is " + names_[i].getValue());
			}
		}
		sWindow_->exitModalState(true);
		sWindow_.reset();
	}
	else if (button == &cancel_) {
		sWindow_->exitModalState(false);
		sWindow_.reset();
	}
}
