/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "PatchView.h"

#include "PatchHolder.h"
#include "ImportFromSynthDialog.h"
#include "AutomaticCategory.h"
#include "PatchDiff.h"
#include "LayeredPatch.h"
#include "LayerCapability.h"
#include "Logger.h"
#include "UIModel.h"
#include "AutoDetection.h"
#include "DataFileLoadCapability.h"

#include <boost/format.hpp>

const char *kAllPatchesFilter = "All patches";

class AutoDetectProgressWindow : public ThreadWithProgressWindow, public midikraft::ProgressHandler {
public:
	AutoDetectProgressWindow(std::vector<midikraft::SynthHolder> synths) :
		ThreadWithProgressWindow("Detecting synth...", true, true), synths_(synths) {
	}

	void run() {

		std::vector < std::shared_ptr<midikraft::SimpleDiscoverableDevice>> synths;
		for (auto s : synths_) {
			synths.push_back(s.device());
		}
		autodetector_.autoconfigure(synths, this);
	}

	virtual bool shouldAbort() const override
	{
		return threadShouldExit();
	}

	virtual void setProgressPercentage(double zeroToOne) override
	{
		setProgress(zeroToOne);
	}

	virtual void onSuccess() override
	{
	}

	virtual void onCancel() override
	{
	}

private:
	std::vector<midikraft::SynthHolder> synths_;
	midikraft::AutoDetection autodetector_;
};


class PatchView::AutoCategorizeWindow : public ThreadWithProgressWindow {
public:
	AutoCategorizeWindow(PatchView *patchView) :
		ThreadWithProgressWindow("Re-running auto categorization...", true, true), patchView_(patchView) {
	}

	void run() {
		// Load the auto category file and re-categorize everything!
		midikraft::AutoCategory::loadFromFile(patchView_->getAutoCategoryFile().getFullPathName().toStdString());
		auto patches = patchView_->database_.getPatches(patchView_->buildFilter(), 0, 100000);
		size_t tick = 0;
		for (auto patch : patches) {
			if (patch.autoCategorizeAgain()) {
				if (threadShouldExit()) break;
				// This was changed, updating database
				SimpleLogger::instance()->postMessage("Updating patch " + String(patch.patch()->patchName()) + " with new categories");
				patchView_->database_.putPatch(UIModel::currentSynth(), patch);
			}
			setProgress(tick++ / (double)patches.size());
		}
		MessageManager::callAsync([this]() {
			patchView_->retrieveFirstPageFromDatabase();
		});
	}

private:
	PatchView *patchView_;
};


PatchView::PatchView(std::vector<midikraft::SynthHolder> const &synths)
	: librarian_(synths), synths_(synths),
	categoryFilters_(predefinedCategories(), [this](CategoryButtons::Category) { retrieveFirstPageFromDatabase(); }, true),
	buttonStrip_(1001, LambdaButtonStrip::Direction::Horizontal)
{
	addAndMakeVisible(importList_);
	importList_.setTextWhenNoChoicesAvailable("No previous import data found");
	importList_.setTextWhenNothingSelected("Click here to filter for a specific import");
	importList_.addListener(this);
	onlyFaves_.setButtonText("Faves");
	onlyFaves_.addListener(this);
	addAndMakeVisible(onlyFaves_);

	currentPatchDisplay_ = std::make_unique<CurrentPatchDisplay>(predefinedCategories(),
		[this](midikraft::PatchHolder &favoritePatch) {
		database_.putPatch(UIModel::currentSynth(), favoritePatch);
		patchButtons_->refresh(true);
	},
		[this](midikraft::PatchHolder &sessionPatch) {
		UIModel::instance()->currentSession_.changedSession();
	});
	addAndMakeVisible(currentPatchDisplay_.get());

	addAndMakeVisible(categoryFilters_);

	LambdaButtonStrip::TButtonMap buttons = {
	{ "autodetect", { 0, "Autodetect synths", [this]() {
		AutoDetectProgressWindow window(synths_);
		window.runThread();
	} } },
	{ "retrieveActiveSynthPatches",{ 1, "Import patches from synth", [this]() {
		retrievePatches();
	} } },
	{ "fetchEditBuffer",{ 2, "Import edit buffer from synth", [this]() {
		retrieveEditBuffer();
	} } },
	{ "loadsysEx", { 3, "Import sysex files from computer", [this]() {
		loadPatches();
	} } },
	{ "showDiff", { 4, "Show patch comparison", [this]() {
		showPatchDiffDialog();
	} } },
	{ "editAutoCategories", { 5, "Edit auto-categories", [this]() {
		if (!URL(getAutoCategoryFile().getFullPathName()).launchInDefaultBrowser()) {
			getAutoCategoryFile().revealToUser();
		}
	} } },
	{ "rerunAutoCategories", { 6, "Rerun auto categorize", [this]() {
		int affected = database_.getPatchesCount(buildFilter());
		if (AlertWindow::showOkCancelBox(AlertWindow::QuestionIcon, "Re-run auto-categorization?",
			"Do you want to rerun the auto-categorization on the currently filtered " + String(affected) + " patches?\n\n"
			"This makes sense if you changed the auto category search strings!\n\n"
			"And don't worry, if you have manually set categories (or manually removed categories that were auto-detected), this information is retained!"
			)) {
			AutoCategorizeWindow window(this);
			window.runThread();
		}
	} } },
	{ "about", { 7, "About", [this]() {
		aboutBox();
	}}}
	};
	patchButtons_ = std::make_unique<PatchButtonPanel>([this](midikraft::PatchHolder &patch) {
		if (UIModel::currentSynth()) {
			selectPatch(*UIModel::currentSynth(), patch);
		}
	});
	buttonStrip_.setButtonDefinitions(buttons);
	addAndMakeVisible(buttonStrip_);
	addAndMakeVisible(patchButtons_.get());
	patchButtons_->setPatchLoader([this](int skip, int limit, std::function<void(std::vector< midikraft::PatchHolder>)> callback) {
		loadPage(skip, limit, callback);
	});	

	// Register for updates
	UIModel::instance()->currentSynth_.addChangeListener(this);
	UIModel::instance()->currentPatch_.addChangeListener(this);
}

PatchView::~PatchView()
{
	UIModel::instance()->currentPatch_.removeChangeListener(this);
	UIModel::instance()->currentSynth_.removeChangeListener(this);
}

void PatchView::changeListenerCallback(ChangeBroadcaster* source)
{
	auto currentSynth = dynamic_cast<CurrentSynth *>(source);
	if (currentSynth) {
		rebuildImportFilterBox();
		retrieveFirstPageFromDatabase();
	}
	else if (dynamic_cast<CurrentPatch *>(source)) {
		currentPatchDisplay_->setCurrentPatch(UIModel::currentSynth(), UIModel::currentPatch());
	}
}

std::vector<CategoryButtons::Category> PatchView::predefinedCategories()
{
	std::vector<CategoryButtons::Category> result;
	for (auto c : midikraft::AutoCategory::predefinedCategoryVector()) {
		result.push_back({ c.category, c.color, c.bitIndex });
	}
	return result;
}

midikraft::PatchDatabase::PatchFilter PatchView::buildFilter() {
	// Transform into real category
	std::set<midikraft::Category> catSelected;
	for (auto c : categoryFilters_.selectedCategories()) {
		catSelected.emplace(c.category, c.color, c.bitIndex);
	}
	return { UIModel::currentSynth(), currentlySelectedSourceUUID(), onlyFaves_.getToggleState(), catSelected };
}

void PatchView::retrieveFirstPageFromDatabase() {
	// First, we need to find out how many patches there are (for the paging control)
	int total = database_.getPatchesCount(buildFilter());
	patchButtons_->setTotalCount(total);
	patchButtons_->refresh(true); // This kicks of loading the first page
}

void PatchView::loadPage(int skip, int limit, std::function<void(std::vector<midikraft::PatchHolder>)> callback) {
	// Kick off loading from the database (could be Internet?)
	midikraft::Synth *loadingForWhich = UIModel::currentSynth();
	database_.getPatchesAsync(buildFilter(), [this, loadingForWhich, callback](std::vector<midikraft::PatchHolder> const &newPatches) {
		// If the synth is still active, refresh the result. Else, just ignore the result
		if (UIModel::currentSynth() == loadingForWhich) {
			callback(newPatches);
		}
	}, skip, limit);
}

void PatchView::resized()
{
	Rectangle<int> area(getLocalBounds());
	auto topRow = area.removeFromTop(100);
	buttonStrip_.setBounds(area.removeFromBottom(60).reduced(8));
	currentPatchDisplay_->setBounds(topRow);
	auto sourceRow = area.removeFromTop(36).reduced(8);
	auto filterRow = area.removeFromTop(40).reduced(10);
	onlyFaves_.setBounds(sourceRow.removeFromRight(80));
	categoryFilters_.setBounds(filterRow);
	importList_.setBounds(sourceRow);
	patchButtons_->setBounds(area.reduced(10));
}

void PatchView::comboBoxChanged(ComboBox* box)
{
	if (box == &importList_) {
		// Same logic as if a new synth had been selected
		retrieveFirstPageFromDatabase();
	}
}

void PatchView::buttonClicked(Button *button)
{
	if (button == &onlyFaves_) {
		retrieveFirstPageFromDatabase();
	}
}

void PatchView::showPatchDiffDialog() {
	if (!compareTarget_.patch() || !UIModel::currentPatch().patch()) {
		// Shouldn't have come here
		return;
	}

	diffDialog_ = std::make_unique<PatchDiff>(UIModel::currentSynth(), compareTarget_, UIModel::currentPatch());

	DialogWindow::LaunchOptions launcher;
	launcher.content = OptionalScopedPointer<Component>(diffDialog_.get(), false);
	launcher.componentToCentreAround = patchButtons_.get();
	launcher.dialogTitle = "Compare two patches";
	launcher.useNativeTitleBar = false;
	launcher.dialogBackgroundColour = Colours::black;
	auto window = launcher.launchAsync();

}

void PatchView::saveCurrentPatchCategories() {
	if (currentPatchDisplay_->getCurrentPatch().patch()) {
		database_.putPatch(UIModel::currentSynth(), currentPatchDisplay_->getCurrentPatch());
		patchButtons_->refresh(false);
	}
}

File PatchView::getAutoCategoryFile() const {
	File appData = File::getSpecialLocation(File::userApplicationDataDirectory).getChildFile("KnobKraft");
	if (!appData.exists()) {
		appData.createDirectory();
	}
	File jsoncFile = appData.getChildFile("automatic_categories.jsonc");
	if (!jsoncFile.exists()) {
		// Create an initial file from the resources!
		FileOutputStream out(jsoncFile);
		out.writeText(midikraft::AutoCategory::defaultJson(), false, false, "\\n");
	}
	return jsoncFile;
}

void PatchView::autoCategorize()
{
}

void PatchView::retrievePatches() {
	midikraft::Synth *activeSynth = UIModel::currentSynth();
	if (activeSynth != nullptr) {
		midikraft::MidiController::instance()->enableMidiInput(activeSynth->midiInput());
		importDialog_ = std::make_unique<ImportFromSynthDialog>(activeSynth,
			[this, activeSynth](midikraft::MidiBankNumber bankNo, midikraft::ProgressHandler *progressHandler) {
			librarian_.startDownloadingAllPatches(
				midikraft::MidiController::instance()->getMidiOutput(activeSynth->midiOutput()),
				activeSynth,
				bankNo,
				progressHandler, [this](std::vector<midikraft::PatchHolder> patchesLoaded) {
				MessageManager::callAsync([this, patchesLoaded]() {
					mergeNewPatches(patchesLoaded);
				});
			});
		}
		);
		DialogWindow::LaunchOptions launcher;
		launcher.content = OptionalScopedPointer<Component>(importDialog_.get(), false);
		launcher.componentToCentreAround = patchButtons_.get();
		launcher.dialogTitle = "Import from Synth";
		launcher.useNativeTitleBar = false;
		auto window = launcher.launchAsync();
	}
	else {
		// Button shouldn't be enabled
		jassert(false);
	}
}


void PatchView::retrieveEditBuffer()
{
	midikraft::Synth *activeSynth = UIModel::currentSynth();
	if (activeSynth != nullptr) {
		librarian_.downloadEditBuffer(midikraft::MidiController::instance()->getMidiOutput(activeSynth->midiOutput()),
			activeSynth,
			nullptr,
			[this](std::vector<midikraft::PatchHolder> patchesLoaded) {
			MessageManager::callAsync([this, patchesLoaded]() {
				mergeNewPatches(patchesLoaded);
			});
		});
	}
	else {
		// Button shouldn't be enabled
		jassert(false);
	}
}

class MergeManyPatchFiles: public ThreadWithProgressWindow, public midikraft::ProgressHandler {
public:
	MergeManyPatchFiles(midikraft::PatchDatabase &database, std::vector<midikraft::PatchHolder> &patchesLoaded, std::function<void(std::vector<midikraft::PatchHolder>)> successHandler) :
		ThreadWithProgressWindow("Uploading...", true, true),
		database_(database), patchesLoaded_(patchesLoaded), finished_(successHandler) {
	}

	void run() {
		std::vector<midikraft::PatchHolder> outNewPatches;
		if (patchesLoaded_.size() == 0) {
			SimpleLogger::instance()->postMessage("No patches contained in data, nothing to upload.");
		}
		else {
			auto numberNew = database_.mergePatchesIntoDatabase(UIModel::currentSynth(), patchesLoaded_, outNewPatches, this);
			if (numberNew > 0) {
				SimpleLogger::instance()->postMessage((boost::format("Retrieved %d new or changed patches from the synth, uploaded to database") % numberNew).str());
				finished_(outNewPatches);
			}
			else {
				SimpleLogger::instance()->postMessage("All patches already known to database");
			}
		}
	}

	virtual bool shouldAbort() const override
	{
		return threadShouldExit();
	}

	virtual void setProgressPercentage(double zeroToOne) override
	{
		setProgress(zeroToOne);
	}

	virtual void onSuccess() override
	{
	}

	virtual void onCancel() override
	{
	}


private:
	midikraft::PatchDatabase &database_;
	std::vector<midikraft::PatchHolder> &patchesLoaded_;
	std::function<void(std::vector<midikraft::PatchHolder>)> finished_;
};

void PatchView::loadPatches() {
	if (UIModel::currentSynth()) {
		auto patches = librarian_.loadSysexPatchesFromDisk(*UIModel::currentSynth());
		if (patches.size() > 0) {
			mergeNewPatches(patches);
		}
	}
}

std::string PatchView::currentlySelectedSourceUUID() {
	if (importList_.getSelectedItemIndex() != -1) {
		return imports_[importList_.getText().toStdString()];
	}
	return "";
}

void PatchView::rebuildImportFilterBox() {
	// Query the database to get a list of all imports that are available for this synth
	auto sources = database_.getImportsList(UIModel::currentSynth());
	imports_.clear();

	StringArray sourceNameList;
	sourceNameList.add(kAllPatchesFilter);
	for (auto source : sources) {
		sourceNameList.add(source.first);
		imports_[source.first] = source.second;
	}
	importList_.clear();
	importList_.addItemList(sourceNameList, 1);
}

void PatchView::mergeNewPatches(std::vector<midikraft::PatchHolder> patchesLoaded) {
	MergeManyPatchFiles backgroundThread(database_, patchesLoaded, [this](std::vector<midikraft::PatchHolder> outNewPatches) {
		// Back to UI thread
		MessageManager::callAsync([this, outNewPatches]() {
			rebuildImportFilterBox();
			// Select this import
			auto info = outNewPatches[0].sourceInfo();
			if (info) {
				for (int i = 0; i < importList_.getNumItems(); i++) {
					if (importList_.getItemText(i).toStdString() == info->toDisplayString(UIModel::currentSynth())) {
							importList_.setSelectedItemIndex(i, sendNotificationAsync);
					}
				}
			}
			retrieveFirstPageFromDatabase();
		});
	});
	backgroundThread.runThread();
}

void PatchView::selectPatch(midikraft::Synth &synth, midikraft::PatchHolder &patch)
{
	// Always refresh the compare target, you just expect it after you clicked it!
	compareTarget_ = UIModel::currentPatch(); // Previous patch is the one we will compare with
	// It could be that we clicked on the patch that is already loaded?
	if (patch.patch() != UIModel::currentPatch().patch()) {
		//SimpleLogger::instance()->postMessage("Selected patch " + patch.patch()->patchName());
		//logger_->postMessage(patch.patch()->patchToTextRaw(true));

		UIModel::instance()->currentPatch_.changeCurrentPatch(patch);
		currentLayer_ = 0;

		// Send out to Synth
		synth.sendPatchToSynth(midikraft::MidiController::instance(), SimpleLogger::instance(), *patch.patch());
	}
	else {
		// Toggle through the layers, if the patch is a layered patch...
		auto layers = std::dynamic_pointer_cast<midikraft::LayeredPatch>(patch.patch());
		if (layers) {
			currentLayer_ = (currentLayer_ + 1) % layers->numberOfLayers();
		}
		auto layerSynth = dynamic_cast<midikraft::LayerCapability *>(&synth);
		if (layerSynth) {
			SimpleLogger::instance()->postMessage((boost::format("Switching to layer %d") % currentLayer_).str());
			layerSynth->switchToLayer(currentLayer_);
		}
	}
}


void PatchView::aboutBox()
{
	String message = "This software is copyright 2020 by Christof Ruch\n\n"
		"Released under dual license, by default under AGPL-3.0, but an MIT licensed version is available on request by the author\n"
		"\n"
		"This software is provided 'as-is,' without any express or implied warranty. In no event shall the author be held liable for any damages arising from the use of this software.\n"
		"\n"
		"Other licenses:\n"
		"This software is build using JUCE, who might want to track your IP address. See https://github.com/WeAreROLI/JUCE/blob/develop/LICENSE.md for details.\n"
		"The boost library is used for parts of this software, see https://www.boost.org/.\n"
		"The installer provided also contains the Microsoft Visual Studio 2017 Redistributable Package.\n"
		;
	AlertWindow::showMessageBox(AlertWindow::InfoIcon, "About", message, "Close");
}

