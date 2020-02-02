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

#include <boost/format.hpp>

const char *kAllPatchesFilter = "All patches";

PatchView::PatchView(std::vector<midikraft::SynthHolder> &synths)
	: librarian_(synths), synths_(synths),
	categoryFilters_({}, [this]() { buttonClicked(nullptr); }, true),
	buttonStrip_(1001, LambdaButtonStrip::Direction::Horizontal),
	compareTarget_(nullptr)
{
	addAndMakeVisible(importList_);
	importList_.setTextWhenNoChoicesAvailable("No previous import data found");
	importList_.setTextWhenNothingSelected("Click here to filter for a specific import");
	importList_.addListener(this);
	onlyFaves_.setButtonText("Faves");
	onlyFaves_.addListener(this);
	addAndMakeVisible(onlyFaves_);

	currentPatchDisplay_ = std::make_unique<CurrentPatchDisplay>([this](midikraft::PatchHolder &favoritePatch) {
		database_.putPatch(UIModel::currentSynth(), favoritePatch);
		patchButtons_->refresh(true);
	},
		[this](midikraft::PatchHolder &sessionPatch) {
		UIModel::instance()->currentSession_.changedSession();
	});
	addAndMakeVisible(currentPatchDisplay_.get());

	addAndMakeVisible(categoryFilters_);

	LambdaButtonStrip::TButtonMap buttons = {
	{ "retrieveActiveSynthPatches",{ 0, "Import patches from synth", [this]() {
		retrievePatches();
	} } },
	{ "loadsysEx", { 1, "Import sysex files from computer", [this]() {
		loadPatches();
	} } },
	{ "showDiff", { 2, "Show patch comparison", [this]() {
		showPatchDiffDialog();
	} } },
/*	{ "migrate", { 3, "Run patch data migration", [this]() {
		database_.runMigration(UIModel::currentSynth());
	} } }*/
	};
	patchButtons_ = std::make_unique<PatchButtonPanel>([this](midikraft::PatchHolder &patch) {
		if (UIModel::currentSynth()) {
			selectPatch(*UIModel::currentSynth(), patch);
		}
	});
	buttonStrip_.setButtonDefinitions(buttons);
	addAndMakeVisible(buttonStrip_);
	addAndMakeVisible(patchButtons_.get());

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
		// Discard currently loaded patches
		library_.clear();
		refreshUI();

		// Kick off loading from the Internet
		midikraft::Synth *loadingForWhich = currentSynth->synth();
		database_.getPatchesAsync(loadingForWhich, [this, loadingForWhich](std::vector<midikraft::PatchHolder> const &newPatches) {
			// If the synth is still active, refresh the result. Else, just ignore the result
			if (UIModel::currentSynth() == loadingForWhich) {
				library_ = newPatches;
				refreshUI();
				rebuildImportFilterBox();
			}
		});
	}
	else if (dynamic_cast<CurrentPatch *>(source)) {
		currentPatchDisplay_->setCurrentPatch(UIModel::currentSynth(), UIModel::currentPatch());
	}
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

std::vector<midikraft::PatchHolder> PatchView::onlyFavorites(std::vector<midikraft::PatchHolder> const &patches, bool reallyOnlyFaves) {
	std::vector<midikraft::PatchHolder> favorites;
	std::remove_copy_if(patches.begin(), patches.end(), std::back_inserter(favorites), [reallyOnlyFaves](midikraft::PatchHolder const &patch) { return !(patch.isFavorite() || !reallyOnlyFaves);  });
	return favorites;
}

std::vector<midikraft::PatchHolder> PatchView::onlyOfCategory(std::vector<midikraft::PatchHolder> const &patches, std::vector<midikraft::Category> const &categories) {
	std::vector<midikraft::PatchHolder> filtered;
	std::remove_copy_if(patches.begin(), patches.end(), std::back_inserter(filtered),
		[&categories](midikraft::PatchHolder const &patch) {
		return std::none_of(categories.begin(), categories.end(), [&](midikraft::Category const &category) { return patch.hasCategory(category); });
	});
	return filtered;
}

std::vector<midikraft::PatchHolder> PatchView::onlyWithSameImport(std::vector<midikraft::PatchHolder> const &patches, std::string const &importDisplayName) {
	if (importDisplayName == kAllPatchesFilter) {
		// Don't filter
		return patches;
	}

	std::vector<midikraft::PatchHolder> filtered;
	std::remove_copy_if(patches.begin(), patches.end(), std::back_inserter(filtered),
		[importDisplayName, this](midikraft::PatchHolder const &patch) {
		return !(patch.sourceInfo() && patch.sourceInfo()->toDisplayString(UIModel::currentSynth()) == importDisplayName);
	});
	return filtered;
}

void PatchView::refreshUI()
{
	std::string selectedImport = kAllPatchesFilter;
	if (importList_.getSelectedItemIndex() != -1) {
		selectedImport = importList_.getText().toStdString();
	}

	if (categoryFilters_.isAtLeastOne()) {
		std::vector<midikraft::Category> categories_selected;
		for (auto c : categoryFilters_.selectedCategories()) {
			categories_selected.emplace_back(c.category, c.color);
		}
		patchButtons_->setPatches(onlyOfCategory(onlyFavorites(onlyWithSameImport(library_, selectedImport), onlyFaves_.getToggleState()), categories_selected));
	}
	else {
		patchButtons_->setPatches(onlyFavorites(onlyWithSameImport(library_, selectedImport), onlyFaves_.getToggleState()));
	}
	currentPatchDisplay_->reset();
}

void PatchView::comboBoxChanged(ComboBox* box)
{
	if (box == &importList_) {
		refreshUI();
	}
}

void PatchView::buttonClicked(Button *button)
{
	refreshUI();
}

void PatchView::showPatchDiffDialog() {
	if (!compareTarget_ || !UIModel::currentPatch()) {
		// Shouldn't have come here
		return;
	}

	diffDialog_ = std::make_unique<PatchDiff>(UIModel::currentSynth(), compareTarget_, UIModel::currentPatch());

	DialogWindow::LaunchOptions launcher;
	launcher.content = OptionalScopedPointer<Component>(diffDialog_.get(), false);
	launcher.componentToCentreAround = patchButtons_.get();
	launcher.dialogTitle = "Compare two patches";
	launcher.useNativeTitleBar = false;
	auto window = launcher.launchAsync();

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

void PatchView::rebuildImportFilterBox() {
	// Add the import information of all patches loaded into the combo box
	std::map<std::string, std::shared_ptr<midikraft::SourceInfo>> sources;
	for (auto patch : library_) {
		if (patch.sourceInfo()) {
			sources.insert_or_assign(patch.sourceInfo()->toDisplayString(UIModel::currentSynth()), patch.sourceInfo());
		}
	}
	StringArray sourceNameList;
	sourceNameList.add(kAllPatchesFilter);
	for (auto source : sources) {
		sourceNameList.add(source.first);
	}
	importList_.clear();
	importList_.addItemList(sourceNameList, 1);
}

void PatchView::mergeNewPatches(std::vector<midikraft::PatchHolder> patchesLoaded) {
	MergeManyPatchFiles backgroundThread(database_, patchesLoaded, [this](std::vector<midikraft::PatchHolder> outNewPatches) {
		for (auto newPatch : outNewPatches) {
			library_.push_back(newPatch);
		}
		rebuildImportFilterBox();
		// Select this import
		auto info = outNewPatches[0].sourceInfo();
		if (info) {
			for (int i = 0; i < importList_.getNumItems(); i++) {
				if (importList_.getItemText(i).toStdString() == info->toDisplayString(UIModel::currentSynth())) {
					MessageManager::callAsync([this, i]() {
						importList_.setSelectedItemIndex(i, sendNotificationAsync); });
				}
			}
		}
		// Back to UI thread
		MessageManager::callAsync([this]() {
			refreshUI();
		});
	});
	backgroundThread.runThread();
}

void PatchView::selectPatch(midikraft::Synth &synth, midikraft::PatchHolder &patch)
{
	// It could be that we clicked on the patch that is already loaded?
	if (&patch != UIModel::currentPatch()) {
		SimpleLogger::instance()->postMessage("Selected patch " + patch.patch()->patchName());
		//logger_->postMessage(patch.patch()->patchToTextRaw(true));

		compareTarget_ = UIModel::currentPatch(); // Previous patch is the one we will compare with
		UIModel::instance()->currentPatch_.changeCurrentPatch(&patch);
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
	}
	auto layerSynth = dynamic_cast<midikraft::LayerCapability *>(&synth);
	if (layerSynth) {
		SimpleLogger::instance()->postMessage((boost::format("Switching to layer %d") % currentLayer_).str());
		layerSynth->switchToLayer(currentLayer_);
	}
}
