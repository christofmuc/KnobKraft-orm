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

PatchView::PatchView(midikraft::PatchDatabase &database, std::vector<midikraft::SynthHolder> const &synths)
	: database_(database), librarian_(synths), synths_(synths),
	categoryFilters_(predefinedCategories(), [this](CategoryButtons::Category) { retrieveFirstPageFromDatabase(); }, true, true),
	synthFilters_({}, [this](CategoryButtons::Category) { retrieveFirstPageFromDatabase();  }, false, true),
	buttonStrip_(1001, LambdaButtonStrip::Direction::Horizontal)
{
	addAndMakeVisible(nameSearchText_);
	nameSearchText_.addListener(this);
	addAndMakeVisible(useNameSearch_);
	useNameSearch_.setButtonText("search in name");
	useNameSearch_.addListener(this);

	addAndMakeVisible(importList_);
	importList_.setTextWhenNoChoicesAvailable("No previous import data found");
	importList_.setTextWhenNothingSelected("Click here to filter for a specific import");
	importList_.addListener(this);

	addAndMakeVisible(dataTypeSelector_);
	dataTypeSelector_.setTextWhenNoChoicesAvailable("This synth does not support different data types");
	dataTypeSelector_.setTextWhenNothingSelected("Click here to show only data of a specific type");
	dataTypeSelector_.addListener(this);

	onlyFaves_.setButtonText("Only Faves");
	onlyFaves_.addListener(this);
	addAndMakeVisible(onlyFaves_);
	showHidden_.setButtonText("Also Hidden");
	showHidden_.addListener(this);
	addAndMakeVisible(showHidden_);
	onlyUntagged_.setButtonText("Only Untagged");
	onlyUntagged_.addListener(this);
	addAndMakeVisible(onlyUntagged_);

	currentPatchDisplay_ = std::make_unique<CurrentPatchDisplay>(predefinedCategories(),
		[this](midikraft::PatchHolder &favoritePatch) {
		database_.putPatch(favoritePatch);
		patchButtons_->refresh(true);
	},
		[this](midikraft::PatchHolder &sessionPatch) {
		ignoreUnused(sessionPatch);
		UIModel::instance()->currentSession_.changedSession();
	});
	addAndMakeVisible(currentPatchDisplay_.get());

	addAndMakeVisible(categoryFilters_);

	advancedSearch_ = std::make_unique<CollapsibleContainer>("Advanced filters", &synthFilters_, false);
	addAndMakeVisible(*advancedSearch_);

	LambdaButtonStrip::TButtonMap buttons = {
	{ "retrieveActiveSynthPatches",{ 0, "Import patches from synth", [this]() {
		retrievePatches();
	} } },
	{ "fetchEditBuffer",{ 1, "Import edit buffer from synth", [this]() {
		retrieveEditBuffer();
	} } },
	{ "loadsysEx", { 2, "Import sysex files from computer", [this]() {
		loadPatches();
	} } },
	{ "showDiff", { 3, "Show patch comparison", [this]() {
		showPatchDiffDialog();
	} } },
	};
	patchButtons_ = std::make_unique<PatchButtonPanel>([this](midikraft::PatchHolder &patch) {
		if (UIModel::currentSynth()) {
			selectPatch(patch);
		}
	});
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
}

PatchView::~PatchView()
{
	UIModel::instance()->currentPatch_.removeChangeListener(this);
	UIModel::instance()->currentSynth_.removeChangeListener(this);
	UIModel::instance()->synthList_.removeChangeListener(this);
}

CategoryButtons::Category synthCategory(midikraft::NamedDeviceCapability *name) {
	return CategoryButtons::Category(name->getName(), Colours::black, 0);
}

void PatchView::changeListenerCallback(ChangeBroadcaster* source)
{
	auto currentSynth = dynamic_cast<CurrentSynth *>(source);
	if (currentSynth) {
		// Select only the newly selected synth in the synth filters
		synthFilters_.setActive({ synthCategory(UIModel::currentSynth()) });

		// Rebuild the other features
		rebuildImportFilterBox();
		rebuildDataTypeFilterBox();
		retrieveFirstPageFromDatabase();
	}
	else if (dynamic_cast<CurrentPatch *>(source)) {
		currentPatchDisplay_->setCurrentPatch(UIModel::currentPatch());
	}
	else if (dynamic_cast<CurrentSynthList *>(source)) {
		rebuildSynthFilters();
	}
}

void PatchView::rebuildSynthFilters() {
	// The available list of synths changed, reset the synth filters
	std::vector<CategoryButtons::Category> synthFilter;
	for (auto synth : UIModel::instance()->synthList_.activeSynths()) {
		synthFilter.push_back(synthCategory(synth.get()));
	}
	synthFilters_.setCategories(synthFilter);
}

std::vector<CategoryButtons::Category> PatchView::predefinedCategories()
{
	std::vector<CategoryButtons::Category> result;
	for (auto c : midikraft::AutoCategory::predefinedCategoryVector()) {
		result.push_back({ c.category, c.color, c.bitIndex });
	}
	return result;
}

void PatchView::textEditorTextChanged(TextEditor&)
{
	if (nameSearchText_.getText().isNotEmpty()) {
		useNameSearch_.setToggleState(true, dontSendNotification);
	}
	retrieveFirstPageFromDatabase();
}

void PatchView::textEditorEscapeKeyPressed(TextEditor&)
{
	nameSearchText_.setText("", true);
	useNameSearch_.setToggleState(false, dontSendNotification);
}

midikraft::PatchDatabase::PatchFilter PatchView::buildFilter() {
	// Transform into real category
	std::set<midikraft::Category> catSelected;
	for (auto c : categoryFilters_.selectedCategories()) {
		catSelected.emplace(c.category, c.color, c.bitIndex);
	}
	bool typeSelected = false;
	int filterType = 0;
	if (dataTypeSelector_.getSelectedId() > 0) {
		typeSelected = true;
		filterType = dataTypeSelector_.getSelectedId() - 1;
	}
	std::string nameFilter = "";
	if (useNameSearch_.getToggleState()) {
		nameFilter = nameSearchText_.getText().toStdString();
	}
	std::map<std::string, midikraft::Synth *> synthMap;
	// Build synth list
	for (auto s : synthFilters_.selectedCategories()) {
		midikraft::SynthHolder synthFound = UIModel::instance()->synthList_.synthByName(s.category);
		if (synthFound.synth()) {
			synthMap[synthFound.synth()->getName()] = synthFound.synth().get(); //TODO these non-shared-pointers make me nervous
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
	// First, we need to find out how many patches there are (for the paging control)
	int total = database_.getPatchesCount(buildFilter());
	patchButtons_->setTotalCount(total);
	patchButtons_->refresh(true); // This kicks of loading the first page
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
	database_.getPatchesAsync(buildFilter(), [this, callback](std::vector<midikraft::PatchHolder> const &newPatches) {
		// TODO - we might want to cancel a running query if the user clicks fast?
		callback(newPatches);
	}, skip, limit);
}

void PatchView::resized()
{
	Rectangle<int> area(getLocalBounds());
	auto topRow = area.removeFromTop(100);
	buttonStrip_.setBounds(area.removeFromBottom(60).reduced(8));
	currentPatchDisplay_->setBounds(topRow.reduced(8));
	auto sourceRow = area.removeFromTop(36).reduced(8);
	auto nameFilterRow = area.removeFromTop(40).reduced(8);
	useNameSearch_.setBounds(nameFilterRow.removeFromRight(100));
	nameSearchText_.setBounds(nameFilterRow);
	auto filterRow = area.removeFromTop(80).reduced(8);
	
	int advancedFilterHeight = advancedSearch_->isOpen() ? (32*2 + 32) : 32;
	auto synthRow = area.removeFromTop(advancedFilterHeight).reduced(8);
	advancedSearch_->setBounds(synthRow);
	
	onlyUntagged_.setBounds(sourceRow.removeFromRight(100));
	showHidden_.setBounds(sourceRow.removeFromRight(100));
	onlyFaves_.setBounds(sourceRow.removeFromRight(100));
	categoryFilters_.setBounds(filterRow);

	dataTypeSelector_.setBounds(sourceRow.removeFromLeft(200));
	importList_.setBounds(sourceRow);
	patchButtons_->setBounds(area.reduced(10));
}

void PatchView::comboBoxChanged(ComboBox* box)
{
	if (box == &importList_ || box == &dataTypeSelector_) {
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
	if (currentPatchDisplay_->getCurrentPatch().patch()) {
		database_.putPatch(currentPatchDisplay_->getCurrentPatch());
		patchButtons_->refresh(false);
	}
}

void PatchView::retrievePatches() {
	midikraft::Synth *activeSynth = UIModel::currentSynth();
	auto midiLocation = dynamic_cast<midikraft::MidiLocationCapability *>(activeSynth);
	if (activeSynth && midiLocation) {
		midikraft::MidiController::instance()->enableMidiInput(midiLocation->midiInput());
		importDialog_ = std::make_unique<ImportFromSynthDialog>(activeSynth,
			[this, activeSynth, midiLocation](MidiBankNumber bankNo, midikraft::ProgressHandler *progressHandler) {
			librarian_.startDownloadingAllPatches(
				midikraft::MidiController::instance()->getMidiOutput(midiLocation->midiOutput()),
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
		launcher.content.set(importDialog_.get(), false);
		launcher.componentToCentreAround = patchButtons_.get();
		launcher.dialogTitle = "Import from Synth";
		launcher.useNativeTitleBar = false;
		auto window = launcher.launchAsync();
		ignoreUnused(window);
	}
	else {
		// Button shouldn't be enabled
		jassert(false);
	}
}


void PatchView::retrieveEditBuffer()
{
	midikraft::Synth *activeSynth = UIModel::currentSynth();
	auto midiLocation = dynamic_cast<midikraft::MidiLocationCapability *>(activeSynth);
	if (activeSynth && midiLocation) {
		librarian_.downloadEditBuffer(midikraft::MidiController::instance()->getMidiOutput(midiLocation->midiOutput()),
			activeSynth,
			nullptr,
			[this](std::vector<midikraft::PatchHolder> patchesLoaded) {
				// There should only be one edit buffer, just check that this is true here
				jassert(patchesLoaded.size() == 1);

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
			auto numberNew = database_.mergePatchesIntoDatabase(patchesLoaded_, outNewPatches, this, midikraft::PatchDatabase::UPDATE_NAME);
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
	if (importList_.getSelectedItemIndex() > 0) {
		return imports_[importList_.getSelectedItemIndex() - 1].id;
	}
	return "";
}

void PatchView::rebuildImportFilterBox() {
	// Query the database to get a list of all imports that are available for this synth
	auto sources = database_.getImportsList(UIModel::currentSynth());
	imports_.clear();

	StringArray sourceNameList;
	sourceNameList.add(kAllPatchesFilter);
	for (const auto& source : sources) {
		sourceNameList.add(source.description);
		imports_.push_back(source);
	}
	importList_.clear();
	importList_.addItemList(sourceNameList, 1);
}

void PatchView::rebuildDataTypeFilterBox() {
	dataTypeSelector_.clear();
	auto dflc = dynamic_cast<midikraft::DataFileLoadCapability *>(UIModel::currentSynth());
	if (dflc) {
		for (size_t i = 0; i < dflc->dataTypeNames().size(); i++) {
			auto typeName = dflc->dataTypeNames()[i];
			if (typeName.canBeSent) {
				dataTypeSelector_.addItem(typeName.name, (int) i + 1);
			}
		}
	}
	
}

void PatchView::mergeNewPatches(std::vector<midikraft::PatchHolder> patchesLoaded) {
	MergeManyPatchFiles backgroundThread(database_, patchesLoaded, [this](std::vector<midikraft::PatchHolder> outNewPatches) {
		// Back to UI thread
		MessageManager::callAsync([this, outNewPatches]() {
			rebuildImportFilterBox();
			// Select this import
			auto info = outNewPatches[0].sourceInfo(); //TODO this will break should I change the logic in the PatchDatabase, this is a mere convention
			if (info) {
				for (int i = 0; i < (int) imports_.size(); i++) {
					if ((imports_[i].name == info->toDisplayString(UIModel::currentSynth())) 
						|| (midikraft::SourceInfo::isEditBufferImport(info) && imports_[i].name == "Edit buffer imports")) // TODO this will break when the display text is changed
					{
							importList_.setSelectedItemIndex(i + 1, dontSendNotification);
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
		patch.synth()->sendPatchToSynth(midikraft::MidiController::instance(), SimpleLogger::instance(), patch.patch());
	}
	else {
		// Toggle through the layers, if the patch is a layered patch...
		auto layers = std::dynamic_pointer_cast<midikraft::LayeredPatch>(patch.patch());
		if (layers) {
			currentLayer_ = (currentLayer_ + 1) % layers->numberOfLayers();
		}
		auto layerSynth = dynamic_cast<midikraft::LayerCapability *>(patch.synth());
		if (layerSynth) {
			SimpleLogger::instance()->postMessage((boost::format("Switching to layer %d") % currentLayer_).str());
			//layerSynth->switchToLayer(currentLayer_);
			MidiBuffer allMessages = layerSynth->layerToSysex(patch.patch(), 1, 0);
			auto location = dynamic_cast<midikraft::MidiLocationCapability *>(patch.synth());
			if (location) {
				SimpleLogger::instance()->postMessage((boost::format("Sending %d messages, total size %d bytes") % allMessages.getNumEvents() % allMessages.data.size()).str());
				midikraft::MidiController::instance()->getMidiOutput(location->midiOutput())->sendBlockOfMessagesNow(allMessages);
			}
			else {
				jassertfalse;
			}
		}
	}
}
