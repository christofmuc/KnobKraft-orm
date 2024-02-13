/*
   Copyright (c) 2021 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "ExportDialog.h"

static std::function<void(midikraft::Librarian::ExportParameters)> sCallback_;

ExportDialog::ExportDialog()
{
	// Properties to edit...
	props_.push_back(std::make_shared<TypedNamedValue>(TypedNamedValue("Sysex format", "", midikraft::Librarian::PROGRAM_DUMPS,
		{ {midikraft::Librarian::PROGRAM_DUMPS, "Sysex format individual program dumps"}, { midikraft::Librarian::EDIT_BUFFER_DUMPS, "Sysex format individual edit buffer dumps" } })));
	props_.push_back(std::make_shared<TypedNamedValue>(TypedNamedValue("File format", "", midikraft::Librarian::MANY_FILES, 
		{ {midikraft::Librarian::MANY_FILES, "Each patch separately into a file"}, { midikraft::Librarian::ZIPPED_FILES, "Each patch separately into a file, but all zipped up" }, 
		{ midikraft::Librarian::ONE_FILE, "One sysex file with all messages" }, { midikraft::Librarian::MID_FILE, "One MIDI file (SMF) to play from a player or DAW" } })));

	parameters_.setProperties(props_);
	addAndMakeVisible(parameters_);

	ok_.onClick = []() { sWindow_->exitModalState(true);  };
	ok_.setButtonText("Export");
	addAndMakeVisible(ok_);

	cancel_.onClick = []() { sWindow_->exitModalState(false); };
	cancel_.setButtonText("Cancel");
	addAndMakeVisible(cancel_);

	// Finally we need a default size
	setBounds(0, 0, 540, 200);
}

void ExportDialog::resized()
{
	auto area = getLocalBounds();
	auto buttonRow = area.removeFromBottom(40).withSizeKeepingCentre(220, 40);
	ok_.setBounds(buttonRow.removeFromLeft(100).reduced(4));
	cancel_.setBounds(buttonRow.removeFromLeft(100).reduced(4));
	parameters_.setBounds(area.reduced(8));
}

midikraft::Librarian::ExportParameters ExportDialog::getResult() 
{
	// Query the property editors
	return { props_.valueByName("Sysex format").getValue(), props_.valueByName("File format").getValue() };
}

static void dialogClosed(int modalResult, ExportDialog* dialog)
{
	if (modalResult == 1 && dialog != nullptr) { // (must check that dialog isn't null in case it was deleted..)
		sCallback_(dialog->getResult());
	}
}

void ExportDialog::showExportDialog(Component *centeredAround, std::string const &title, std::function<void(midikraft::Librarian::ExportParameters)> callback)
{
	if (!sExportDialog_) {
		sExportDialog_ = std::make_unique<ExportDialog>();
	}
	sCallback_ = callback;

	DialogWindow::LaunchOptions launcher;
	launcher.content.set(sExportDialog_.get(), false);
	launcher.componentToCentreAround = centeredAround;
	launcher.dialogTitle = title;
	launcher.useNativeTitleBar = false;
	sWindow_ = launcher.launchAsync();
	ModalComponentManager::getInstance()->attachCallback(sWindow_, ModalCallbackFunction::forComponent(dialogClosed, sExportDialog_.get()));

}

void ExportDialog::shutdown()
{
	sExportDialog_.reset();
}

std::unique_ptr<ExportDialog> ExportDialog::sExportDialog_;

juce::DialogWindow * ExportDialog::sWindow_;


