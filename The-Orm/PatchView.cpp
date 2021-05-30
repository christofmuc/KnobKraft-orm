/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "PatchView.h"

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
const char *kAllDataTypesFilter = "All types";

class PatchViewCenter : public Component
{
public:
	ComboBox *importList_;
	CategoryButtons *categoryFilters_;
	CollapsibleContainer  *advancedSearch_;
	ToggleButton *onlyFaves_;
	ToggleButton *showHidden_;
	ToggleButton *onlyUntagged_;
	Label *patchLabel_;
	PatchButtonPanel *patchButtons_;

	virtual void resized() {
		Rectangle<int> area(getBounds());
		auto normalFilter = area.removeFromTop(32 * 2 + 24 + 3 * 8).reduced(8);
		auto sourceRow = normalFilter.removeFromTop(24);
		auto filterRow = normalFilter.withTrimmedTop(8); // 32 per row
		int advancedFilterHeight = advancedSearch_->isOpen() ? (24 + 24 + 2 * 32) : 24;
		advancedSearch_->setBounds(area.removeFromTop(advancedFilterHeight).withTrimmedLeft(8).withTrimmedRight(8));
		onlyUntagged_->setBounds(sourceRow.removeFromRight(100));
		showHidden_->setBounds(sourceRow.removeFromRight(100));
		onlyFaves_->setBounds(sourceRow.removeFromRight(100));
		categoryFilters_->setBounds(filterRow);
		importList_->setBounds(sourceRow);
		patchButtons_->setBounds(area.withTrimmedRight(8).withTrimmedLeft(8));
	}
};

PatchView::PatchView(midikraft::PatchDatabase &database, std::vector<midikraft::SynthHolder> const &synths, std::shared_ptr<midikraft::AutomaticCategory> detector)
	: database_(database), librarian_(synths), synths_(synths), automaticCategories_(detector), 
	patchListTree_(database, [this](String id) { selectImportByID(id); }),
	categoryFilters_({}, [this](CategoryButtons::Category) { retrieveFirstPageFromDatabase(); }, true, true),
	advancedFilters_(this),
	buttonStrip_(1001, LambdaButtonStrip::Direction::Horizontal)
{
	addAndMakeVisible(importList_);
	importList_.setTextWhenNoChoicesAvailable("No previous import data found");
	importList_.setTextWhenNothingSelected("Click here to filter for a specific import");
	importList_.addListener(this);

	onlyFaves_.setButtonText("Only Faves");
	onlyFaves_.addListener(this);
	addAndMakeVisible(onlyFaves_);
	showHidden_.setButtonText("Also Hidden");
	showHidden_.addListener(this);
	addAndMakeVisible(showHidden_);
	onlyUntagged_.setButtonText("Only Untagged");
	onlyUntagged_.addListener(this);
	addAndMakeVisible(onlyUntagged_);

	currentPatchDisplay_ = std::make_unique<CurrentPatchDisplay>(database_, predefinedCategories(),
		[this](std::shared_ptr<midikraft::PatchHolder> favoritePatch) {
		database_.putPatch(*favoritePatch);
		patchButtons_->refresh(true);
	}
	);

	patchButtons_ = std::make_unique<PatchButtonPanel>([this](midikraft::PatchHolder& patch) {
		if (UIModel::currentSynth()) {
			selectPatch(patch);
		}
	});

	advancedSearch_ = std::make_unique<CollapsibleContainer>("Advanced filters", &advancedFilters_, false);
	addAndMakeVisible(*advancedSearch_);

	// Refactoring helper PatchViewCenter
	PatchViewCenter* center = new PatchViewCenter();
	center->importList_ = &importList_;
	center->categoryFilters_ = &categoryFilters_;
	center->advancedSearch_ = advancedSearch_.get();
	center->onlyFaves_ = &onlyFaves_;
	center->showHidden_ = &showHidden_;
	center->onlyUntagged_ = &onlyUntagged_;
	center->patchLabel_ = &patchLabel_;
	center->patchButtons_ = patchButtons_.get();

	splitters_ = std::make_unique<SplitteredComponent>("PatchViewSplitter",
		SplitteredEntry{ &patchListTree_, 15, 5, 40 },
		SplitteredEntry{ center, 70, 40, 90 },
		SplitteredEntry{ currentPatchDisplay_.get(), 15, 5, 40}, true);
	addAndMakeVisible(splitters_.get());

	addAndMakeVisible(categoryFilters_);

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
	addAndMakeVisible(patchButtons_.get());

	patchButtons_->setPatchLoader([this](int skip, int limit, std::function<void(std::vector< midikraft::PatchHolder>)> callback) {
		loadPage(skip, limit, callback);
	});

	rebuildSynthFilters();

	// Register for updates
	UIModel::instance()->currentSynth_.addChangeListener(this);
	UIModel::instance()->currentPatch_.addChangeListener(this);
	UIModel::instance()->synthList_.addChangeListener(this);
	UIModel::instance()->categoriesChanged.addChangeListener(this);
}

PatchView::~PatchView()
{
	UIModel::instance()->categoriesChanged.removeChangeListener(this);
	UIModel::instance()->currentPatch_.removeChangeListener(this);
	UIModel::instance()->currentSynth_.removeChangeListener(this);
	UIModel::instance()->synthList_.removeChangeListener(this);
}

CategoryButtons::Category synthCategory(midikraft::NamedDeviceCapability *name) {
	return CategoryButtons::Category(name->getName(), Colours::black);
}

void PatchView::changeListenerCallback(ChangeBroadcaster* source)
{
	auto currentSynth = dynamic_cast<CurrentSynth *>(source);
	if (currentSynth) {
		categoryFilters_.setCategories(predefinedCategories());

		// Select only the newly selected synth in the synth filters
		if (UIModel::currentSynth()) {
			advancedFilters_.synthFilters_.setActive({ synthCategory(UIModel::currentSynth()) });
		}

		// Rebuild the other features
		rebuildImportFilterBox();
		rebuildDataTypeFilterBox();
		retrieveFirstPageFromDatabase();
	}
	else if (dynamic_cast<CurrentPatch *>(source)) {
		currentPatchDisplay_->setCurrentPatch(std::make_shared<midikraft::PatchHolder>(UIModel::currentPatch()));
	}
	else if (dynamic_cast<CurrentSynthList *>(source)) {
		rebuildSynthFilters();
	}
	else if (source == &UIModel::instance()->categoriesChanged) {
		categoryFilters_.setCategories(predefinedCategories());
		retrieveFirstPageFromDatabase();
	}
}

void PatchView::rebuildSynthFilters() {
	// The available list of synths changed, reset the synth filters
	std::vector<CategoryButtons::Category> synthFilter;
	for (auto synth : UIModel::instance()->synthList_.activeSynths()) {
		synthFilter.push_back(synthCategory(synth.get()));
	}
	advancedFilters_.synthFilters_.setCategories(synthFilter);
	if (UIModel::currentSynth()) {
		advancedFilters_.synthFilters_.setActive({ synthCategory(UIModel::currentSynth()) });
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

void PatchView::textEditorTextChanged(TextEditor&)
{
	if (advancedFilters_.nameSearchText_.getText().isNotEmpty()) {
		advancedFilters_.useNameSearch_.setToggleState(true, dontSendNotification);
	}
	retrieveFirstPageFromDatabase();
}

void PatchView::textEditorEscapeKeyPressed(TextEditor&)
{
	advancedFilters_.nameSearchText_.setText("", true);
	advancedFilters_.useNameSearch_.setToggleState(false, dontSendNotification);
}

midikraft::PatchDatabase::PatchFilter PatchView::buildFilter() {
	// Transform into real category
	std::set<midikraft::Category> catSelected;
	for (auto c : categoryFilters_.selectedCategories()) {
		for (auto dc : database_.getCategories()) {
			if (dc.category() == c.category) {
				catSelected.emplace(dc);
				break;
			}
		}
	}
	bool typeSelected = false;
	int filterType = 0;
	if (advancedFilters_.dataTypeSelector_.getSelectedId() > 1) { // 0 is empty drop down, and 1 is "All data types"
		typeSelected = true;
		filterType = advancedFilters_.dataTypeSelector_.getSelectedId() - 2;
	}
	std::string nameFilter = "";
	if (advancedFilters_.useNameSearch_.getToggleState()) {
		if (!advancedFilters_.nameSearchText_.getText().startsWith("!")) {
			nameFilter = advancedFilters_.nameSearchText_.getText().toStdString();
		}
	}
	std::map<std::string, std::weak_ptr<midikraft::Synth>> synthMap;
	// Build synth list
	for (auto s : advancedFilters_.synthFilters_.selectedCategories()) {
		midikraft::SynthHolder synthFound = UIModel::instance()->synthList_.synthByName(s.category);
		if (synthFound.synth()) {
			synthMap[synthFound.synth()->getName()] = synthFound.synth();
		}
	}
	return { synthMap,
		currentlySelectedSourceUUID(),
		nameFilter,
		onlyFaves_.getToggleState(),
		typeSelected,
		filterType,
		showHidden_.getToggleState(),
		onlyUntagged_.getToggleState(),
		catSelected };
}

void PatchView::retrieveFirstPageFromDatabase() {
	// If at least one synth is selected, build and run the query. Never run a query against all synths from this code
	if (!advancedFilters_.synthFilters_.selectedCategories().empty()) {
		// First, we need to find out how many patches there are (for the paging control)
		int total = database_.getPatchesCount(buildFilter());
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
	database_.getPatchesAsync(buildFilter(), [this, callback](midikraft::PatchDatabase::PatchFilter const filter, std::vector<midikraft::PatchHolder> const &newPatches) {
		// Discard the result when there is a newer filter - another thread will be working on a better result!
		if (buildFilter() != filter)
			return;

		// Check if a client-side filter is active (python based)
		String advancedQuery = advancedFilters_.nameSearchText_.getText();
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

	if (area.getWidth() > area.getHeight() * 1.5) {
		// Landscape layout		
		buttonStrip_.setBounds(area.removeFromBottom(60).reduced(8));
		splitters_->setBounds(area);
	}
	else {
		// Portrait
		auto topRow = area.removeFromTop(100);
		buttonStrip_.setBounds(area.removeFromBottom(60).reduced(8));
		//currentPatchDisplay_->setBounds(topRow);
		splitters_->setBounds(area);
	}
}

void PatchView::comboBoxChanged(ComboBox* box)
{
	if (box == &importList_ || box == &advancedFilters_.dataTypeSelector_) {
		// Same logic as if a new synth had been selected
		retrieveFirstPageFromDatabase();
	}
}

void PatchView::buttonClicked(Button *button)
{
	ignoreUnused(button);
	retrieveFirstPageFromDatabase();
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
			int deleted = database_.deletePatches(buildFilter());
			AlertWindow::showMessageBox(AlertWindow::InfoIcon, "Patches deleted", (boost::format("%d patches deleted from database") % deleted).str());
			rebuildImportFilterBox();
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
		rebuildImportFilterBox();
		retrieveFirstPageFromDatabase();
	}
}

int PatchView::totalNumberOfPatches()
{
	return database_.getPatchesCount(buildFilter());
}

void PatchView::selectFirstPatch()
{
	patchButtons_->selectFirst();
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
	if (!advancedFilters_.synthFilters_.selectedCategories().empty()) {
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
	if (!advancedFilters_.synthFilters_.selectedCategories().empty()) {
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

void PatchView::selectImportByID(String id) {
	std::string description;
	for (auto import : imports_) {
		if (import.id == id) {
			description = import.description;
			break;
		}
	}
	for (int j = 0; j < importList_.getNumItems(); j++) {
		if (importList_.getItemText(j).toStdString() == description) {
			importList_.setSelectedItemIndex(j, sendNotificationAsync);
			break;
		}
	}
}

std::string PatchView::currentlySelectedSourceUUID() {
	if (importList_.getSelectedItemIndex() > 0) {
		std::string selectedItemText = importList_.getText().toStdString();
		for (auto import : imports_) {
			if (import.description == selectedItemText) {
				return import.id;
			}
		}
		jassertfalse;
	}
	return "";
}

void PatchView::rebuildImportFilterBox() {
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
	importList_.clear();
	importList_.addItemList(sourceNameList, 1);
}

void PatchView::rebuildDataTypeFilterBox() {
	advancedFilters_.dataTypeSelector_.clear();
	auto dflc = midikraft::Capability::hasCapability<midikraft::DataFileLoadCapability>(UIModel::instance()->currentSynth_.smartSynth());
	if (dflc) {
		StringArray typeNameList;
		typeNameList.add(kAllDataTypesFilter);
		for (size_t i = 0; i < dflc->dataTypeNames().size(); i++) {
			auto typeName = dflc->dataTypeNames()[i];
			if (typeName.canBeSent) {
				typeNameList.add(typeName.name);
			}
		}
		advancedFilters_.dataTypeSelector_.addItemList(typeNameList, 1);
	}
}

void PatchView::mergeNewPatches(std::vector<midikraft::PatchHolder> patchesLoaded) {
	MergeManyPatchFiles backgroundThread(database_, patchesLoaded, [this](std::vector<midikraft::PatchHolder> outNewPatches) {
		// Back to UI thread
		MessageManager::callAsync([this, outNewPatches]() {
			rebuildImportFilterBox();
			if (outNewPatches.size() > 0) {
				// Select this import
				auto info = outNewPatches[0].sourceInfo(); //TODO this will break should I change the logic in the PatchDatabase, this is a mere convention
				if (info) {
					for (int i = 0; i < (int)imports_.size(); i++) {
						if ((imports_[i].id == info->md5(UIModel::currentSynth()))
							|| (midikraft::SourceInfo::isEditBufferImport(info) && imports_[i].name == "Edit buffer imports")) // TODO this will break when the display text is changed
						{
							// Search for the import in the sorted list
							for (int j = 0; j < importList_.getNumItems(); j++) {
								if (importList_.getItemText(j).toStdString() == imports_[i].description) {
									importList_.setSelectedItemIndex(j, dontSendNotification);
									break;
								}
							}
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

PatchView::AdvancedFilterPanel::AdvancedFilterPanel(PatchView *patchView) :
	synthFilters_({}, [patchView](CategoryButtons::Category) { patchView->retrieveFirstPageFromDatabase();  }, false, true)
{
	addAndMakeVisible(nameSearchText_);
	nameSearchText_.addListener(patchView);
	addAndMakeVisible(useNameSearch_);
	useNameSearch_.setButtonText("search in name");
	useNameSearch_.addListener(patchView);
	addAndMakeVisible(synthFilters_);
	addAndMakeVisible(dataTypeSelector_);
	dataTypeSelector_.setTextWhenNoChoicesAvailable("This synth does not support different data types");
	dataTypeSelector_.setTextWhenNothingSelected("Click here to show only data of a specific type");
	dataTypeSelector_.addListener(patchView);
}

void PatchView::AdvancedFilterPanel::resized()
{
	auto area = getLocalBounds();
	auto nameFilterRow = area.removeFromTop(24);
	dataTypeSelector_.setBounds(nameFilterRow.removeFromLeft(200).withTrimmedRight(16));
	useNameSearch_.setBounds(nameFilterRow.removeFromRight(100));
	nameSearchText_.setBounds(nameFilterRow);
	synthFilters_.setBounds(area);
}
