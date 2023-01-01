/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "CreateNewAdaptationDialog.h"

#include "GenericAdaptation.h"

namespace knobkraft {

	std::unique_ptr<CreateNewAdaptationDialog> CreateNewAdaptationDialog::dialog_;

	CreateNewAdaptationDialog::CreateNewAdaptationDialog() : ok_("Ok"), cancel_("Cancel")
	{
		addAndMakeVisible(template_);
		addAndMakeVisible(ok_);
		addAndMakeVisible(cancel_);
		addAndMakeVisible(text_);
		addAndMakeVisible(basedOn_);
		basedOn_.setText("Based on", dontSendNotification);
		text_.setText("Please select a build-in adaptation as a template. This will be copied into your user adaptations folder for you to modify", dontSendNotification);
		text_.setColour(Label::textColourId, getLookAndFeel().findColour(Label::ColourIds::textWhenEditingColourId));

		ok_.addListener(this);
		cancel_.addListener(this);

		StringArray templateList;
		for (auto const &a : GenericAdaptation::getAllBuiltinSynthNames()) {
			templateList.add(a);
		}
						
		template_.addItemList(templateList, 1);
		template_.setSelectedId(1, dontSendNotification);
		setSize(400, 200);
	}

	void CreateNewAdaptationDialog::resized()
	{
		auto area = getLocalBounds();
		text_.setBounds(area.removeFromTop(60).reduced(8));
		auto buttons = area.removeFromBottom(40).reduced(8);
		cancel_.setBounds(buttons.removeFromRight(88).withTrimmedRight(8));
		ok_.setBounds(buttons.removeFromRight(88).withTrimmedRight(8));
		basedOn_.setBounds(area.removeFromLeft(100).withTrimmedRight(8));
		template_.setBounds(area.withSizeKeepingCentre(200, 30));
	}

	void CreateNewAdaptationDialog::showDialog(Component *center)
	{
		dialog_ = std::make_unique<CreateNewAdaptationDialog>();

		DialogWindow::LaunchOptions launcher;
		launcher.content.set(dialog_.get(), false);
		launcher.componentToCentreAround = center;
		launcher.dialogTitle = "Create new Adaptation";
		launcher.useNativeTitleBar = false;
		auto window = launcher.launchAsync();
		ignoreUnused(window);
	}

	bool CreateNewAdaptationDialog::createNewAdaptation() {
		int selected = template_.getSelectedItemIndex();
		if (selected == -1) {
			jassertfalse;
			return false;
		}

		bool worked = GenericAdaptation::breakOut(template_.getText().toStdString());
		if (worked) {
			AlertWindow::showMessageBox(AlertWindow::InfoIcon, "Copied", "The selected adaptation was copied into the directory %s.\n\nYou can open it in a Python editor and first change the name to start making a new synth adapation");
		}
		else {
			AlertWindow::showMessageBox(AlertWindow::WarningIcon, "Error", "Program error: Something went wrong while copying the adaptation source code.");
		}
		return worked;
	}

	void CreateNewAdaptationDialog::buttonClicked(Button* button)
	{
		if (button == &ok_) {

			if (!createNewAdaptation()) {
				return;
			}

			if (DialogWindow* dw = findParentComponentOfClass<DialogWindow>()) {
				dw->exitModalState(1);
			}
		}
		else if (button == &cancel_) {
			if (DialogWindow* dw = findParentComponentOfClass<DialogWindow>()) {
				dw->exitModalState(1);
			}
		}
		MessageManager::callAsync([]() {
			// Prevent a (wrong) memory leak to be detected
			dialog_.reset();
		});
	}

}
