/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "PatchView.h"

#include "PatchSearchComponent.h"

#include "PatchHolder.h"
#include "ImportFromSynthDialog.h"
#include "AutomaticCategory.h"
#include "PatchDiff.h"
#include "LayeredPatchCapability.h"
#include "LayerCapability.h"
#include "Logger.h"
#include "UIModel.h"
#include "AutoDetection.h"
#include "DataFileLoadCapability.h"
#include "ScriptedQuery.h"
#include "ProgressHandlerWindow.h"

#include "GenericAdaptation.h" //TODO For the Python runtime. That should probably go to its own place, as Python now is used for more than the GenericAdaptation

#include <boost/format.hpp>
#include "PatchInterchangeFormat.h"
#include "Settings.h"
#include "ReceiveManualDumpWindow.h"
#include "ExportDialog.h"

const char *kAllPatchesFilter = "All patches";

PatchView::PatchView(midikraft::PatchDatabase &database, std::vector<midikraft::SynthHolder> const &synths, std::shared_ptr<midikraft::AutomaticCategory> detector)
	: database_(database), librarian_(synths), synths_(synths), automaticCategories_(detector), 
	patchListTree_(database, [this](String id) { patchSearch_->selectImportByID(id); }),
	buttonStrip_(1001, LambdaButtonStrip::Direction::Horizontal)
{
	patchButtons_ = std::make_unique<PatchButtonPanel>([this](midikraft::PatchHolder& patch) {
		if (UIModel::currentSynth()) {
			selectPatch(patch);
		}
	});

	currentPatchDisplay_ = std::make_unique<CurrentPatchDisplay>(database_, predefinedCategories(),
		[this](std::shared_ptr<midikraft::PatchHolder> favoritePatch) {
		database_.putPatch(*favoritePatch);
		patchButtons_->refresh(true);
	}
	);

	patchSearch_ = std::make_unique<PatchSearchComponent>(this, patchButtons_.get(), database_);

	splitters_ = std::make_unique<SplitteredComponent>("PatchViewSplitter",
		SplitteredEntry{ &patchListTree_, 15, 5, 40 },
		SplitteredEntry{ patchSearch_.get(), 70, 40, 90 },
		SplitteredEntry{ currentPatchDisplay_.get(), 15, 5, 40}, true);
	addAndMakeVisible(splitters_.get());

	LambdaButtonStrip::TButtonMap buttons = {
	{ "retrieveActiveSynthPatches",{ "Import patches from synth", [this]() {
		retrievePatches();
	} } },
	{ "fetchEditBuffer",{ "Import edit buffer from synth", [this]() {
		retrieveEditBuffer();
	} } },
	{ "receiveManualDump",{ "Receive manual dump", [this]() {
		receiveManualDump();
	},  } },
	{ "loadsysEx", { "Import sysex files from computer", [this]() {
		loadPatches();
	} } },
	{ "exportSysex", { "Export into sysex files", [this]() {
		exportPatches();
	}}},
	{ "exportPIF", { "Export into PIF", [this]() {
		createPatchInterchangeFile();
	} } },
	{ "showDiff", { "Show patch comparison", [this]() {
		showPatchDiffDialog();
	} } },
	};
	buttonStrip_.setButtonDefinitions(buttons);
	addAndMakeVisible(buttonStrip_);
	
	patchButtons_->setPatchLoader([this](int skip, int limit, std::function<void(std::vector< midikraft::PatchHolder>)> callback) {
		loadPage(skip, limit, callback);
	});

	patchSearch_->rebuildSynthFilters();

	// Register for updates
	UIModel::instance()->currentPatch_.addChangeListener(this);
}

PatchView::~PatchView()
{
	UIModel::instance()->categoriesChanged.removeChangeListener(this);
	UIModel::instance()->currentPatch_.removeChangeListener(this);
	UIModel::instance()->currentSynth_.removeChangeListener(this);
	UIModel::instance()->synthList_.removeChangeListener(this);
}

void PatchView::changeListenerCallback(ChangeBroadcaster* source)
{
	if (dynamic_cast<CurrentPatch *>(source)) {
		currentPatchDisplay_->setCurrentPatch(std::make_shared<midikraft::PatchHolder>(UIModel::currentPatch()));
	}
}

std::vector<CategoryButtons::Category> PatchView::predefinedCategories()
{
	std::vector<CategoryButtons::Category> result;
	for (const auto& c : database_.getCategories()) {
		if (c.def()->isActive) {
			result.emplace_back(c.category(), c.color());
		}
	}
	return result;
}

void PatchView::retrieveFirstPageFromDatabase() {
	// If at least one synth is selected, build and run the query. Never run a query against all synths from this code
	if (patchSearch_->atLeastOneSynth()) {
		// First, we need to find out how many patches there are (for the paging control)
		int total = database_.getPatchesCount(patchSearch_->buildFilter());
		patchButtons_->setTotalCount(total);
		patchButtons_->refresh(true); // This kicks of loading the first page
	}
}

void PatchView::hideCurrentPatch()
{
	selectNextPatch();
	currentPatchDisplay_->toggleHide();
}

void PatchView::favoriteCurrentPatch()
{
	currentPatchDisplay_->toggleFavorite();
}

void PatchView::selectPreviousPatch()
{
	patchButtons_->selectPrevious();
}

void PatchView::selectNextPatch()
{
	patchButtons_->selectNext();
}

void PatchView::loadPage(int skip, int limit, std::function<void(std::vector<midikraft::PatchHolder>)> callback) {
	// Kick off loading from the database (could be Internet?)
	database_.getPatchesAsync(patchSearch_->buildFilter(), [this, callback](midikraft::PatchDatabase::PatchFilter const filter, std::vector<midikraft::PatchHolder> const &newPatches) {
		// Discard the result when there is a newer filter - another thread will be working on a better result!
		if (patchSearch_->buildFilter() != filter)
			return;

		// Check if a client-side filter is active (python based)
		String advancedQuery = patchSearch_->advancedTextSearch();
		if (advancedQuery.startsWith("!") && knobkraft::GenericAdaptation::hasPython()) {
			// Bang start indicates python predicate to evaluate instead of just a name query!
			ScriptedQuery query;
			// Drop the first character (!)
			auto filteredPatches = query.filterByPredicate(advancedQuery.substring(1).toStdString(), newPatches);
			callback(filteredPatches);
		}
		else {
			callback(newPatches);
		}
	}, skip, limit);
}

void PatchView::resized()
{
	Rectangle<int> area(getLocalBounds());

	/*if (area.getWidth() > area.getHeight() * 1.5)*/ {
		// Landscape layout		
		buttonStrip_.setBounds(area.removeFromBottom(LAYOUT_LARGE_LINE_SPACING + LAYOUT_INSET_NORMAL).reduced(LAYOUT_INSET_NORMAL));
		splitters_->setBounds(area);
	}
	/*else {
		// Portrait
		auto topRow = area.removeFromTop(100);
		buttonStrip_.setBounds(area.removeFromBottom(60).reduced(8));
		//currentPatchDisplay_->setBounds(topRow);
		splitters_->setBounds(area);
	}*/
}

void PatchView::showPatchDiffDialog() {
	if (!compareTarget_.patch() || !UIModel::currentPatch().patch()) {
		// Shouldn't have come here
		return;
	}

	if (compareTarget_.synth()->getName() != UIModel::currentPatch().synth()->getName()) {
		// Should have come either
		SimpleLogger::instance()->postMessage((boost::format("Can't compare patch %s of synth %s with patch %s of synth %s") %
			UIModel::currentPatch().patch()->name() % UIModel::currentPatch().synth()->getName()
			% compareTarget_.patch()->name() % compareTarget_.synth()->getName()).str());
		return;
	}

	diffDialog_ = std::make_unique<PatchDiff>(UIModel::currentPatch().synth(), compareTarget_, UIModel::currentPatch());

	DialogWindow::LaunchOptions launcher;
	launcher.content.set(diffDialog_.get(), false);
	launcher.componentToCentreAround = patchButtons_.get();
	launcher.dialogTitle = "Compare two patches";
	launcher.useNativeTitleBar = false;
	launcher.dialogBackgroundColour = Colours::black;
	auto window = launcher.launchAsync();
	ignoreUnused(window);
}

void PatchView::saveCurrentPatchCategories() {
	if (currentPatchDisplay_->getCurrentPatch()->patch()) {
		database_.putPatch(*currentPatchDisplay_->getCurrentPatch());
		patchButtons_->refresh(false);
	}
}

class LibrarianProgressWindow : public ProgressHandlerWindow {
public:
	LibrarianProgressWindow(midikraft::Librarian &librarian) : ProgressHandlerWindow("Import patches from Synth", "..."), librarian_(librarian) {
	}

	// Override this from the ThreadWithProgressWindow to understand closing with cancel button!
	virtual void threadComplete(bool userPressedCancel) override {
		if (userPressedCancel) {
			// Make sure to destroy any stray MIDI callback handlers, else we'll get into trouble when we retry the operation
			librarian_.clearHandlers();
		}
	}

private:
	midikraft::Librarian &librarian_;
};

void PatchView::retrievePatches() {
	auto activeSynth = UIModel::instance()->currentSynth_.smartSynth();
	auto device = std::dynamic_pointer_cast<midikraft::DiscoverableDevice>(activeSynth);
	auto midiLocation = midikraft::Capability::hasCapability<midikraft::MidiLocationCapability>(activeSynth);
	std::shared_ptr<ProgressHandlerWindow> progressWindow = std::make_shared<LibrarianProgressWindow>(librarian_);
	if (activeSynth /*&& device->wasDetected()*/) {
		midikraft::MidiController::instance()->enableMidiInput(midiLocation->midiInput());
		importDialog_ = std::make_unique<ImportFromSynthDialog>(activeSynth.get(),
			[this, progressWindow, activeSynth, midiLocation](std::vector<MidiBankNumber> bankNo) {
			if (!bankNo.empty()) {
				progressWindow->launchThread();
				librarian_.startDownloadingAllPatches(
					midikraft::MidiController::instance()->getMidiOutput(midiLocation->midiOutput()),
					activeSynth,
					bankNo,
					progressWindow.get(), [this, progressWindow](std::vector<midikraft::PatchHolder> patchesLoaded) {
					progressWindow->signalThreadShouldExit();
					MessageManager::callAsync([this, patchesLoaded]() {
						auto enhanced = autoCategorize(patchesLoaded);
						mergeNewPatches(enhanced);
					});
				});
			}
		}
		);
		DialogWindow::LaunchOptions launcher;
		launcher.content.set(importDialog_.get(), false);
		launcher.componentToCentreAround = patchButtons_.get();
		launcher.dialogTitle = "Import from Synth";
		launcher.useNativeTitleBar = false;
		auto window = launcher.launchAsync();
		ignoreUnused(window);
	}
	else {
		// Button shouldn't be enabled
	}
}

std::vector<midikraft::PatchHolder> PatchView::autoCategorize(std::vector<midikraft::PatchHolder> const &patches) {
	for (auto p : patches) {
		p.autoCategorizeAgain(automaticCategories_);
	}
	return patches;
}


void PatchView::retrieveEditBuffer()
{
	auto activeSynth = UIModel::instance()->currentSynth_.smartSynth();
	auto midiLocation = midikraft::Capability::hasCapability<midikraft::MidiLocationCapability>(activeSynth);
	if (activeSynth && midiLocation) {
		librarian_.downloadEditBuffer(midikraft::MidiController::instance()->getMidiOutput(midiLocation->midiOutput()),
			activeSynth,
			nullptr,
			[this](std::vector<midikraft::PatchHolder> patchesLoaded) {
			// There should only be one edit buffer, just check that this is true here
			jassert(patchesLoaded.size() == 1);

			patchesLoaded = autoCategorize(patchesLoaded);

			// Set a specific "EditBufferImport" source for those patches retrieved directly from the edit buffer
			auto now = Time::getCurrentTime();
			auto editBufferSource = std::make_shared<midikraft::FromSynthSource>(now);
			for (auto &p : patchesLoaded) {
				p.setSourceInfo(editBufferSource);
			}

			// Off to the UI thread (because we will update the UI)
			MessageManager::callAsync([this, patchesLoaded]() {
				mergeNewPatches(patchesLoaded);
			});
		});
	}
}

void PatchView::deletePatches()
{
	int totalAffected = totalNumberOfPatches();
	if (AlertWindow::showOkCancelBox(AlertWindow::QuestionIcon, (boost::format("Delete all %d patches matching current filter") % totalAffected).str(),
		(boost::format("Warning, there is no undo operation. Do you really want to delete the %d patches matching the current filter?\n\n"
			"They will be gone forever, unless you use a backup!") % totalAffected).str())) {
		if (AlertWindow::showOkCancelBox(AlertWindow::WarningIcon, "Do you know what you are doing?",
			"Are you sure?", "Yes", "No")) {
			int deleted = database_.deletePatches(patchSearch_->buildFilter());
			AlertWindow::showMessageBox(AlertWindow::InfoIcon, "Patches deleted", (boost::format("%d patches deleted from database") % deleted).str());
			patchSearch_->rebuildImportFilterBox();
			retrieveFirstPageFromDatabase();
		}
	}
}

void PatchView::reindexPatches() {
	// We do reindex all patches of the currently selected synth. It does not make sense to reindex less than that.
	auto currentSynth = UIModel::instance()->currentSynth_.smartSynth();
	if (!currentSynth) return;
	auto filter = midikraft::PatchDatabase::allForSynth(currentSynth);

	int totalAffected = database_.getPatchesCount(filter);
	if (AlertWindow::showOkCancelBox(AlertWindow::QuestionIcon, (boost::format("Do you want to reindex all %d patches for synth %s?") % totalAffected % currentSynth->getName()).str(),
		(boost::format("This will reindex the %d patches with the current fingerprinting algorithm.\n\n"
			"Hopefully this will get rid of duplicates properly, but if there are duplicates under multiple names you'll end up with a somewhat random result which name is chosen for the de-duplicated patch.\n")
			% totalAffected).str())) {
		std::string backupName = database_.makeDatabaseBackup("-before-reindexing");
		SimpleLogger::instance()->postMessage((boost::format("Created database backup at %s") % backupName).str());
		int countAfterReindexing = database_.reindexPatches(filter);
		if (countAfterReindexing != -1) {
			// No error, display user info
			if (totalAffected > countAfterReindexing) {
				AlertWindow::showMessageBox(AlertWindow::InfoIcon, "Reindexing patches successful",
					(boost::format("The reindexing reduced the number of patches from %d to %d due to deduplication.") % totalAffected % countAfterReindexing).str());
			}
			else {
				AlertWindow::showMessageBox(AlertWindow::InfoIcon, "Reindexing patches successful", "The count of patches did not change, but they are now indexed with the correct fingerprint and should stop duplicating themselves.");
			}
		}
		else {
			AlertWindow::showMessageBox(AlertWindow::WarningIcon, "Error reindexing patches", "There was an error reindexing the patches selected. View the log for more details");

		}
		patchSearch_->rebuildImportFilterBox();
		retrieveFirstPageFromDatabase();
	}
}

int PatchView::totalNumberOfPatches()
{
	return database_.getPatchesCount(patchSearch_->buildFilter());
}

void PatchView::selectFirstPatch()
{
	patchButtons_->selectFirst();
}

midikraft::PatchDatabase::PatchFilter PatchView::currentFilter()
{
	return patchSearch_->buildFilter();
}

class MergeManyPatchFiles : public ProgressHandlerWindow {
public:
	MergeManyPatchFiles(midikraft::PatchDatabase &database, std::vector<midikraft::PatchHolder> &patchesLoaded, std::function<void(std::vector<midikraft::PatchHolder>)> successHandler) :
		ProgressHandlerWindow("Storing in database", "Merging new patches into database..."),
		database_(database), patchesLoaded_(patchesLoaded), finished_(successHandler) {
	}

	virtual void run() override {
		std::vector<midikraft::PatchHolder> outNewPatches;
		if (patchesLoaded_.size() == 0) {
			SimpleLogger::instance()->postMessage("No patches contained in data, nothing to upload.");
		}
		else {
			auto numberNew = database_.mergePatchesIntoDatabase(patchesLoaded_, outNewPatches, this, midikraft::PatchDatabase::UPDATE_NAME | midikraft::PatchDatabase::UPDATE_CATEGORIES | midikraft::PatchDatabase::UPDATE_FAVORITE);
			if (numberNew > 0) {
				SimpleLogger::instance()->postMessage((boost::format("Retrieved %d new or changed patches from the synth, uploaded to database") % numberNew).str());
				finished_(outNewPatches);
			}
			else {
				SimpleLogger::instance()->postMessage("All patches already known to database");
				finished_({});
			}
		}
	}

	virtual void onCancel() override
	{
		//Forgot why, but we should not signal the thread to exit as in the default implementation of ProgressHandlerWindow
	}


private:
	midikraft::PatchDatabase &database_;
	std::vector<midikraft::PatchHolder> &patchesLoaded_;
	std::function<void(std::vector<midikraft::PatchHolder>)> finished_;
};

void PatchView::receiveManualDump() {
	auto synthToReceiveFrom = UIModel::instance()->currentSynth_.smartSynth();

	if (synthToReceiveFrom) {
		// We need to start a listener thread, and display a waiting dialog box with an end button all the while...
		ReceiveManualDumpWindow receiveDumpBox(UIModel::instance()->currentSynth_.smartSynth());

		receiveDumpBox.runThread();

		auto messagesReceived = receiveDumpBox.result();
		if (messagesReceived.size() > 0) {
			// Try to load via Librarian
			auto patches = librarian_.loadSysexPatchesManualDump(synthToReceiveFrom, messagesReceived, automaticCategories_);
			if (patches.size() > 0) {
				auto enhanced = autoCategorize(patches);
				mergeNewPatches(enhanced);
			}
		}
	}
}

void PatchView::loadPatches() {
	if (UIModel::currentSynth()) {
		auto patches = librarian_.loadSysexPatchesFromDisk(UIModel::instance()->currentSynth_.smartSynth(), automaticCategories_);
		if (patches.size() > 0) {
			auto enhanced = autoCategorize(patches);
			mergeNewPatches(enhanced);
		}
	}
}

void PatchView::exportPatches()
{
	// If at least one synth is selected, build and run the query. Never run a query against all synths from this code
	if (patchSearch_->atLeastOneSynth()) {
		loadPage(0, -1, [this](std::vector<midikraft::PatchHolder> patches) {
			ExportDialog::showExportDialog(this, [this, patches](midikraft::Librarian::ExportParameters params) {
				librarian_.saveSysexPatchesToDisk(params, patches);
			});
		});
	}
}

void PatchView::updateLastPath() {
	if (lastPathForPIF_.empty()) {
		// Read from settings
		lastPathForPIF_ = Settings::instance().get("lastPatchInterchangePath", "");
		if (lastPathForPIF_.empty()) {
			// Default directory
			lastPathForPIF_ = File::getSpecialLocation(File::userDocumentsDirectory).getFullPathName().toStdString();
		}
	}
}

void PatchView::createPatchInterchangeFile()
{
	// If at least one synth is selected, build and run the query. Never run a query against all synths from this code
	if (patchSearch_->atLeastOneSynth()) {
		loadPage(0, -1, [this](std::vector<midikraft::PatchHolder> patches) {
			updateLastPath();
			FileChooser pifChooser("Please enter the name of the Patch Interchange Format file to create...", File(lastPathForPIF_), "*.json");
			if (pifChooser.browseForFileToSave(true)) {
				midikraft::PatchInterchangeFormat::save(patches, pifChooser.getResult().getFullPathName().toStdString());
				lastPathForPIF_ = pifChooser.getResult().getFullPathName().toStdString();
				Settings::instance().set("lastPatchInterchangePath", lastPathForPIF_);
			}
		});
	}
}

StringArray PatchView::sourceNameList() {
	// Query the database to get a list of all imports that are available for this synth
	auto sources = database_.getImportsList(UIModel::currentSynth());
	imports_.clear();

	StringArray sourceNameList;
	for (const auto& source : sources) {
		sourceNameList.add(source.description);
		imports_.push_back(source);
	}
	sourceNameList.sortNatural();
	sourceNameList.insert(0, kAllPatchesFilter);
	return sourceNameList;
}

void PatchView::mergeNewPatches(std::vector<midikraft::PatchHolder> patchesLoaded) {
	MergeManyPatchFiles backgroundThread(database_, patchesLoaded, [this](std::vector<midikraft::PatchHolder> outNewPatches) {
		// Back to UI thread
		MessageManager::callAsync([this, outNewPatches]() {
			patchSearch_->rebuildImportFilterBox();
			if (outNewPatches.size() > 0) {
				// Select this import
				auto info = outNewPatches[0].sourceInfo(); //TODO this will break should I change the logic in the PatchDatabase, this is a mere convention
				if (info) {
					for (int i = 0; i < (int)imports_.size(); i++) {
						if ((imports_[i].id == info->md5(UIModel::currentSynth()))
							|| (midikraft::SourceInfo::isEditBufferImport(info) && imports_[i].name == "Edit buffer imports")) // TODO this will break when the display text is changed
						{
							patchSearch_->selectImportByDescription(imports_[i].description);
						}
					}
				}
			}
			retrieveFirstPageFromDatabase();
		});
	});
	backgroundThread.runThread();
}

void PatchView::selectPatch(midikraft::PatchHolder &patch)
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
		patch.synth()->sendDataFileToSynth(patch.patch(), nullptr);
	}
	else {
		// Toggle through the layers, if the patch is a layered patch...
		auto layers = midikraft::Capability::hasCapability<midikraft::LayeredPatchCapability>(patch.patch());
		if (layers) {
			currentLayer_ = (currentLayer_ + 1) % layers->numberOfLayers();
		}
		auto layerSynth = midikraft::Capability::hasCapability<midikraft::LayerCapability>(patch.smartSynth());
		if (layerSynth) {
			SimpleLogger::instance()->postMessage((boost::format("Switching to layer %d") % currentLayer_).str());
			//layerSynth->switchToLayer(currentLayer_);
			auto allMessages = layerSynth->layerToSysex(patch.patch(), 1, 0);
			auto location = midikraft::Capability::hasCapability<midikraft::MidiLocationCapability>(patch.smartSynth());
			if (location) {
				int totalSize = 0;
				totalSize = std::accumulate(allMessages.cbegin(), allMessages.cend(), totalSize, [](int acc, MidiMessage const &m) { return m.getRawDataSize() + acc; });
				SimpleLogger::instance()->postMessage((boost::format("Sending %d messages, total size %d bytes") % allMessages.size() % totalSize).str());
				patch.synth()->sendBlockOfMessagesToSynth(location->midiOutput(), allMessages);
			}
			else {
				jassertfalse;
			}
		}
	}
}

