/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "CreateNewAdaptionDialog.h"

#include "BundledAdaption.h"
#include "GenericAdaption.h"

namespace knobkraft {

	std::unique_ptr<CreateNewAdaptionDialog> CreateNewAdaptionDialog::dialog_;

	CreateNewAdaptionDialog::CreateNewAdaptionDialog() : ok_("Ok"), cancel_("Cancel")
	{
		addAndMakeVisible(template_);
		addAndMakeVisible(ok_);
		addAndMakeVisible(cancel_);
		addAndMakeVisible(text_);
		addAndMakeVisible(basedOn_);
		basedOn_.setText("Based on", dontSendNotification);
		text_.setText("Please select a build-in adaption as a template. This will be copied into your user adaptions folder for you to modify", dontSendNotification);
		text_.setColour(Label::textColourId, getLookAndFeel().findColour(Label::ColourIds::textWhenEditingColourId));

		ok_.addListener(this);
		cancel_.addListener(this);

		StringArray templateList;
		for (auto const &a : gBundledAdaptions()) {
			templateList.add(a.synthName);
		}
						
		template_.addItemList(templateList, 1);
		template_.setSelectedId(1, dontSendNotification);
		setSize(400, 200);
	}

	void CreateNewAdaptionDialog::resized()
	{
		auto area = getLocalBounds();
		text_.setBounds(area.removeFromTop(60).reduced(8));
		auto buttons = area.removeFromBottom(40).reduced(8);
		cancel_.setBounds(buttons.removeFromRight(88).withTrimmedRight(8));
		ok_.setBounds(buttons.removeFromRight(88).withTrimmedRight(8));
		basedOn_.setBounds(area.removeFromLeft(100).withTrimmedRight(8));
		template_.setBounds(area.withSizeKeepingCentre(200, 30));
	}

	void CreateNewAdaptionDialog::showDialog(Component *center)
	{
		dialog_ = std::make_unique<CreateNewAdaptionDialog>();

		DialogWindow::LaunchOptions launcher;
		launcher.content.set(dialog_.get(), false);
		launcher.componentToCentreAround = center;
		launcher.dialogTitle = "Create new Adaption";
		launcher.useNativeTitleBar = false;
		auto window = launcher.launchAsync();
		ignoreUnused(window);
	}

	bool CreateNewAdaptionDialog::createNewAdaption() {
		int selected = template_.getSelectedItemIndex();
		if (selected == -1) {
			jassertfalse;
			return false;
		}

		auto adaption = gBundledAdaptions()[selected];

		auto dir = GenericAdaption::getAdaptionDirectory();

		// Copy out source code
		File target = dir.getChildFile(adaption.pythonModuleName + ".py");
		if (target.exists()) {
			juce::AlertWindow::showMessageBox(AlertWindow::AlertIconType::WarningIcon, "File exists", "There is already a file for this adaption, which we will not overwrite.");
			return false;
		}
		
		FileOutputStream out(target);
#if WIN32
		out.writeText(adaption.adaptionSourceCode, false, false, "\r\n");
#else
		out.writeText(adaption.adaptionSourceCode, false, false, "\n");
#endif
		return true;
	}

	void CreateNewAdaptionDialog::buttonClicked(Button* button)
	{
		if (button == &ok_) {

			if (!createNewAdaption()) {
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
		MessageManager::callAsync([this]() {
			// Prevent a (wrong) memory leak to be detected
			dialog_.reset();
		});
	}

}
