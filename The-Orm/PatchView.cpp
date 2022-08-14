/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "PatchView.h"

#include "PatchSearchComponent.h"
#include "InsetBox.h"
#include "LambdaLayoutBox.h"

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
#include "LibrarianProgressWindow.h"

#include "GenericAdaptation.h" //TODO For the Python runtime. That should probably go to its own place, as Python now is used for more than the GenericAdaptation

#include <boost/format.hpp>
#include "fmt/format.h"
#include "PatchInterchangeFormat.h"
#include "Settings.h"
#include "ReceiveManualDumpWindow.h"
#include "ExportDialog.h"
#include "SynthBank.h"

#include "LayoutConstants.h"

const char *kAllPatchesFilter = "All patches";

PatchView::PatchView(midikraft::PatchDatabase &database, std::vector<midikraft::SynthHolder> const &synths)
	: database_(database), librarian_(synths), synths_(synths), 
	patchListTree_(database, synths),
	buttonStrip_(1001, LambdaButtonStrip::Direction::Horizontal)
{
	patchListTree_.onSynthBankSelected = [this](std::shared_ptr<midikraft::Synth> synth, MidiBankNumber bank) {
		setSynthBankFilter(synth, bank);
	};
	patchListTree_.onImportListSelected = [this](String id) {
		setImportListFilter(id);
	};
	patchListTree_.onUserListSelected= [this](String id) {
		setUserListFilter(id);
	};
	patchListTree_.onUserListChanged = [this](String id) {
		if (listFilterID_ == id) {
			retrieveFirstPageFromDatabase();
		}
	};
	patchListTree_.onPatchSelected = [this](midikraft::PatchHolder patch) {
		selectPatch(patch, false);
	};

	patchButtons_ = std::make_unique<PatchButtonPanel>([this](midikraft::PatchHolder& patch) {
		if (UIModel::currentSynth()) {
			selectPatch(patch, true);
		}
	});

	currentPatchDisplay_ = std::make_unique<CurrentPatchDisplay>(database_, predefinedCategories(),
		[this](std::shared_ptr<midikraft::PatchHolder> favoritePatch) {
		database_.putPatch(*favoritePatch);
		patchButtons_->refresh(true);
	}
	);
	currentPatchDisplay_->onCurrentPatchClicked = [this](std::shared_ptr<midikraft::PatchHolder> patch) {
		if (patch) {
			selectPatch(*patch, true);
		}
	};

	synthBank_ = std::make_unique<SynthBankPanel>(database_, this);

	patchSearch_ = std::make_unique<PatchSearchComponent>(this, patchButtons_.get(), database_);

	auto box = new LambdaLayoutBox();
	box->onResized = [this](Component* box) {
		auto area = box->getLocalBounds();
		recycleBin_.setBounds(area.removeFromBottom(LAYOUT_LINE_HEIGHT * 2).withTrimmedBottom(LAYOUT_INSET_NORMAL));
		patchListTree_.setBounds(area.reduced(LAYOUT_INSET_NORMAL));
	};
	addAndMakeVisible(box);
	box->addAndMakeVisible(&patchListTree_);
	box->addAndMakeVisible(&recycleBin_);
	recycleBin_.onClicked = [this]() {
		AlertWindow::showMessageBox(AlertWindow::InfoIcon, "Delete functionality", "The trash can is a drag and drop target you can use to delete patches or patch list entries - "
			"just drag a patch or a list entry onto the trash can and drop it.\nDeleting patch list entries will be done immediately,"
			" but deleting patches will ask for confirmation, as this is a destructive operation.");
	};
	recycleBin_.onItemDropped = [this](var item) {
		String dropItemString = item;
		auto infos = midikraft::PatchHolder::dragInfoFromString(dropItemString.toStdString());
		//SimpleLogger::instance()->postMessage("Item dropped: " + dropItemString);
		deleteSomething(infos);
	};
	
	auto centerBox = new LambdaLayoutBox();
	centerBox->onResized = [this](Component* box) {
		auto area = box->getLocalBounds();
		patchSearch_->setBounds(area.removeFromLeft(area.getWidth() / 4 * 3));
		synthBank_->setBounds(area.reduced(LAYOUT_INSET_NORMAL));
	};
	centerBox->addAndMakeVisible(*synthBank_);
	centerBox->addAndMakeVisible(*patchSearch_);

	splitters_ = std::make_unique<SplitteredComponent>("PatchViewSplitter",
		SplitteredEntry{ box, 15, 5, 40 },
		SplitteredEntry{ centerBox, 50, 40, 90 },
		SplitteredEntry{ currentPatchDisplay_.get(), 15, 5, 40}, true);
	addAndMakeVisible(splitters_.get());

	addAndMakeVisible(recycleBin_);

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
		loadPage(skip, limit, currentFilter(), callback);
	});

	// Register for updates
	UIModel::instance()->currentPatch_.addChangeListener(this);
}

PatchView::~PatchView()
{
	UIModel::instance()->currentPatch_.removeChangeListener(this);
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
	// First, we need to find out how many patches there are (for the paging control)
	int total = database_.getPatchesCount(currentFilter());
	patchButtons_->setTotalCount(total);
	patchButtons_->refresh(true); // This kicks of loading the first page
}

std::shared_ptr<midikraft::PatchList>  PatchView::retrieveListFromDatabase(midikraft::ListInfo const& info)
{
	if (info.id.empty()) {
		return nullptr;
	}

	std::map<std::string, std::weak_ptr<midikraft::Synth>> synths;
	for (auto synth : synths_) {
		synths.emplace(synth.getName(), synth.synth());
	}
	return database_.getPatchList(info, synths);
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

void PatchView::loadPage(int skip, int limit, midikraft::PatchFilter const& filter, std::function<void(std::vector<midikraft::PatchHolder>)> callback) {
	// Kick off loading from the database (could be Internet?)
	database_.getPatchesAsync(filter, [this, callback](midikraft::PatchFilter const filter, std::vector<midikraft::PatchHolder> const &newPatches) {
		// Discard the result when there is a newer filter - another thread will be working on a better result!
		/*if (currentFilter() != filter)
			return;*/

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

void PatchView::loadSynthBankFromDatabase(std::shared_ptr<midikraft::Synth> synth, MidiBankNumber bank, std::string const& bankId)
{
	loadPage(0, -1, bankFilter(synth, bankId), [this, synth, bank, bankId](std::vector<midikraft::PatchHolder> patches) {
		SimpleLogger::instance()->postMessage(fmt::format("Bank of {} patches retrieved from database", patches.size()));

		// We need to patch the patches' position, so they represent the bank loaded and not their original position on import whenever that was!
		//TODO - this should possible go into the PatchDatabase code. But it is a load option?
		int i = 0;
		for (auto& patch : patches) {
			patch.setBank(bank);
			patch.setPatchNumber(MidiProgramNumber::fromZeroBaseWithBank(bank, i++));
		}

		// Load the bank info from the database as well for the timestamp
		std::map<std::string, std::weak_ptr<midikraft::Synth>> synths;
		synths[synth->getName()] = synth;
		midikraft::ListInfo info;
		info.id = bankId; 
		info.name = ""; // Don't care for the name
		auto fullInfo = database_.getPatchList(info, synths);
		if (fullInfo) {
			auto bankList = std::dynamic_pointer_cast<midikraft::SynthBank>(fullInfo);
			if (bankList) {
				UIModel::instance()->synthBank.setSynthBank(bankList);
				//TODO could be transported via UIModel?
				synthBank_->setBank(bankList, PatchButtonInfo::DefaultDisplay);
			} 
		}
		else {
			SimpleLogger::instance()->postMessage("Program Error: Invalid synth bank, not stored in database. Can't load into panel");
			return;
		}
	});
}

void PatchView::retrieveBankFromSynth(midikraft::SynthBank bankToRetrieve, std::function<void()> finishedHandler)
{
	auto device = std::dynamic_pointer_cast<midikraft::DiscoverableDevice>(bankToRetrieve.synth());
	auto location = midikraft::Capability::hasCapability<midikraft::MidiLocationCapability>(bankToRetrieve.synth());
	if (location) {
		if (location->channel().isValid() && device->wasDetected()) {
			// We can offer to download the bank from the synth, or rather just do it!
			auto progressWindow = std::make_shared<LibrarianProgressWindow>(librarian_);
			if (bankToRetrieve.synth() /*&& device->wasDetected()*/) {
				midikraft::MidiController::instance()->enableMidiInput(location->midiInput());
				progressWindow->launchThread();
				progressWindow->setMessage((boost::format("Importing %s from %s...") % bankToRetrieve.synth()->friendlyBankName(bankToRetrieve.bankNumber()) % bankToRetrieve.synth()->getName()).str());
				librarian_.startDownloadingAllPatches(
					midikraft::MidiController::instance()->getMidiOutput(location->midiOutput()),
					bankToRetrieve.synth(),
					bankToRetrieve.bankNumber(),
					progressWindow.get(), [this, progressWindow, bankToRetrieve, finishedHandler](std::vector<midikraft::PatchHolder> patchesLoaded) {
						progressWindow->signalThreadShouldExit();
						MessageManager::callAsync([this, patchesLoaded, bankToRetrieve, finishedHandler]() {
							SimpleLogger::instance()->postMessage("Retrieved " + String(patchesLoaded.size()) + " patches from synth");
							// First make sure all patches are stored in the database
							auto enhanced = autoCategorize(patchesLoaded);
							mergeNewPatches(enhanced); //This is actually async!, should be reflected in the name. Maybe I should open a progress dialog here?
							// Then store the list of them in the database
							auto retrievedBank = std::make_shared<midikraft::SynthBank>(bankToRetrieve.synth(), bankToRetrieve.bankNumber(), juce::Time::getCurrentTime());
							retrievedBank->setPatches(patchesLoaded);
							database_.putPatchList(retrievedBank);
							// We need to mark something as "active in synth" together with position in the patch_in_list table, so we now when we can program change to the patch
							// instead of sending the sysex
							patchListTree_.refreshAllUserLists();
							loadSynthBankFromDatabase(bankToRetrieve.synth(), bankToRetrieve.bankNumber(), bankToRetrieve.id());
							if (finishedHandler) {
								finishedHandler();
							}
							});
					});
			}
		}
		else {
			AlertWindow::showMessageBox(juce::AlertWindow::AlertIconType::InfoIcon, "Synth not connected", "For bank management of banks stored in the synth, make sure the synth is connected and detected correctly. Use the MIDI setup to make sure you have connectivity and a green bar!");
		}
	}
	else {
		SimpleLogger::instance()->postMessage("Invalid operation - cannot retrieve bank from synth that has no MIDI connectivity implemented");
	}
}

void PatchView::sendBankToSynth(std::shared_ptr<midikraft::SynthBank> bankToSend, std::function<void()> finishedHandler)
{
	if (!bankToSend) return;

	auto device = std::dynamic_pointer_cast<midikraft::DiscoverableDevice>(bankToSend->synth());
	auto location = midikraft::Capability::hasCapability<midikraft::MidiLocationCapability>(bankToSend->synth());
	if (location) {
		if (location->channel().isValid() && device->wasDetected()) {
			auto progressWindow = std::make_shared<LibrarianProgressWindow>(librarian_);
			if (bankToSend->synth() /*&& device->wasDetected()*/) {
				midikraft::MidiController::instance()->enableMidiInput(location->midiInput());
				progressWindow->launchThread();
				librarian_.sendBankToSynth(*bankToSend, false, progressWindow.get(), [this, bankToSend, finishedHandler](bool completed) {
					if (completed) {
						bankToSend->clearDirty();
						if (finishedHandler) {
							finishedHandler();
						}
					}
					else {
						AlertWindow::showMessageBox(juce::AlertWindow::AlertIconType::WarningIcon, "Incomplete bank update", "The bank update did not finish, you might or not have a partial bank transferred!");
					}
				});
			}
		}
		else {
			AlertWindow::showMessageBox(juce::AlertWindow::AlertIconType::InfoIcon, "Synth not connected", "For bank management of banks stored in the synth, make sure the synth is connected and detected correctly. Use the MIDI setup to make sure you have connectivity and a green bar!");
		}
	}
	else {
		SimpleLogger::instance()->postMessage("Invalid operation - cannot send bank to synth that has no MIDI connectivity implemented");
	}
}

void PatchView::setSynthBankFilter(std::shared_ptr<midikraft::Synth> synth, MidiBankNumber bank) {
	midikraft::SynthBank bankList(synth, bank, juce::Time());
	// Check if this synth bank has ever been loaded
	std::map<std::string, std::weak_ptr<midikraft::Synth>> synths;
	synths[synth->getName()] = synth;
	if (database_.doesListExist(bankList.id())) {
		// It does, so we can safely load and display it
		loadSynthBankFromDatabase(synth, bank, bankList.id());
	}
	else {
		// No, first time ever - offer the user to download from the synth if connected
		retrieveBankFromSynth(bankList, {});
	}
}

void PatchView::setImportListFilter(String filter)
{
	listFilterID_ = "";
	sourceFilterID_ = filter.toStdString();
	retrieveFirstPageFromDatabase();
}

void PatchView::setUserListFilter(String filter)
{
	listFilterID_ = filter.toStdString();
	sourceFilterID_ = "";
	retrieveFirstPageFromDatabase();
}

void PatchView::deleteSomething(nlohmann::json const& infos)
{
	if (infos.contains("drag_type") && infos["drag_type"].is_string()) {
		std::string drag_type = infos["drag_type"];
		if (drag_type == "PATCH") {
			// A patch was dropped and is to be deleted - but ask the user!
			std::string patchName = infos["patch_name"];
			if (AlertWindow::showOkCancelBox(AlertWindow::WarningIcon, "Delete patch from database",
				"Do you really want to delete the patch " + patchName + " from the database? There is no undo!")) {
				database_.deletePatches(infos["synth"], { infos["md5"] });
				SimpleLogger::instance()->postMessage("Deleted patch " + patchName + " from database");
				patchListTree_.refreshAllUserLists();
				patchButtons_->refresh(true);
			}
			return;
		}
		else if (drag_type == "PATCH_IN_LIST") {
			// Just remove that patch from the list in question
			std::string list_id = infos["list_id"];
			std::string patch_name = infos["patch_name"];
			std::string list_name = infos["list_name"];
			database_.removePatchFromList(list_id, infos["synth"], infos["md5"], infos["order_num"]);
			SimpleLogger::instance()->postMessage("Removed patch " + patch_name + " from list " + list_name);
			patchListTree_.refreshUserList(list_id);
			if (listFilterID_ == list_id) {
				retrieveFirstPageFromDatabase();
			}
			return;
		}
		else if (drag_type == "LIST") {
			std::string list_id = infos["list_id"];
			std::string list_name = infos["list_name"];
			if (AlertWindow::showOkCancelBox(AlertWindow::QuestionIcon, "Delete list from database",
				"Do you really want to delete the list " + list_name + " from the database? There is no undo!")) {
				database_.deletePatchlist(midikraft::ListInfo{ list_id, list_name });
				SimpleLogger::instance()->postMessage("Deleted list " + list_name);
				if (listFilterID_ == list_id) {
				}
				patchListTree_.refreshAllUserLists();
			}
			return;
		}
	}
	SimpleLogger::instance()->postMessage("Program error - unknow drop type dropped on recycle bin!");
}

void PatchView::retrievePatches() {
	auto activeSynth = UIModel::instance()->currentSynth_.smartSynth();
	auto device = std::dynamic_pointer_cast<midikraft::DiscoverableDevice>(activeSynth);
	auto midiLocation = midikraft::Capability::hasCapability<midikraft::MidiLocationCapability>(activeSynth);
	std::shared_ptr<ProgressHandlerWindow> progressWindow = std::make_shared<LibrarianProgressWindow>(librarian_);
	if (activeSynth /*&& device->wasDetected()*/) {
		midikraft::MidiController::instance()->enableMidiInput(midiLocation->midiInput());
		importDialog_ = std::make_unique<ImportFromSynthDialog>(activeSynth,
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
		p.autoCategorizeAgain(database_.getCategorizer());
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

			if (patchesLoaded.size() == 1) {
				SimpleLogger::instance()->postMessage((boost::format("Edit buffer from synth is program %s") % patchesLoaded[0].name()).str());
			}

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
			int deleted = database_.deletePatches(currentFilter());
			AlertWindow::showMessageBox(AlertWindow::InfoIcon, "Patches deleted", (boost::format("%d patches deleted from database") % deleted).str());
			//TODO refresh import Filter
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
		//TODO refresh import filter
		retrieveFirstPageFromDatabase();
	}
}

int PatchView::totalNumberOfPatches()
{
	return database_.getPatchesCount(currentFilter());
}

void PatchView::selectFirstPatch()
{
	patchButtons_->selectFirst();
}

midikraft::PatchFilter PatchView::currentFilter()
{
	auto filter = patchSearch_->getFilter();
	filter.importID = sourceFilterID_;
	filter.listID = listFilterID_;
	return filter;
}

midikraft::PatchFilter PatchView::bankFilter(std::shared_ptr<midikraft::Synth> synth, std::string const& listID)
{
	// We want to load all patches for this synth that are in the bank list given
	auto filter = database_.allForSynth(synth);
	filter.showHidden = true; // If there are hidden patches, we need to know!
	filter.importID = "";
	filter.listID = listID;
	filter.orderBy = midikraft::PatchOrdering::Order_by_Place_in_List;
	return filter;
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
			auto patches = librarian_.loadSysexPatchesManualDump(synthToReceiveFrom, messagesReceived, database_.getCategorizer());
			if (patches.size() > 0) {
				auto enhanced = autoCategorize(patches);
				mergeNewPatches(enhanced);
			}
		}
	}
}

void PatchView::loadPatches() {
	if (UIModel::currentSynth()) {
		auto patches = librarian_.loadSysexPatchesFromDisk(UIModel::instance()->currentSynth_.smartSynth(), database_.getCategorizer());
		if (patches.size() > 0) {
			auto enhanced = autoCategorize(patches);
			mergeNewPatches(enhanced);
		}
	}
}

class BulkImportPIP : public ThreadWithProgressWindow {
public:
	BulkImportPIP(File directory, midikraft::PatchDatabase &db, std::shared_ptr<midikraft::AutomaticCategory> detector) 
		: ThreadWithProgressWindow("Importing patch archives...", true, true), directory_(directory), db_(db), detector_(detector) {
	}

	virtual void run() {
		std::map<std::string, std::shared_ptr<midikraft::Synth>> synths;
		for (auto synth : UIModel::instance()->synthList_.allSynths()) {
			synths[synth.getName()] = synth.synth();
		}

		Array<File> pips;
		directory_.findChildFiles(pips, File::TypesOfFileToFind::findFiles, false, "*.json");
		double count = 0.0;
		for (auto const &pip : pips) {
			if (threadShouldExit())
				break;

			if (pip.existsAsFile()) {
				auto patches = midikraft::PatchInterchangeFormat::load(synths, pip.getFullPathName().toStdString(), detector_);
				std::vector<midikraft::PatchHolder> outNewPatches;
				auto numberNew = db_.mergePatchesIntoDatabase(patches, outNewPatches, nullptr, midikraft::PatchDatabase::UPDATE_NAME | midikraft::PatchDatabase::UPDATE_CATEGORIES | midikraft::PatchDatabase::UPDATE_FAVORITE);
				if (numberNew > 0) {
					SimpleLogger::instance()->postMessage("Loaded " + String(numberNew) + " additional patches from file " + pip.getFullPathName());
				}
			}

			count += 1.0;
			setProgress(count / pips.size());
		}
	}

private:
	File directory_;
	midikraft::PatchDatabase& db_;
	std::shared_ptr<midikraft::AutomaticCategory> detector_;
};

void PatchView::bulkImportPIP(File directory) {
	BulkImportPIP bulk(directory, database_, database_.getCategorizer());

	bulk.runThread();

	retrieveFirstPageFromDatabase();
}

void PatchView::exportPatches()
{
	loadPage(0, -1, currentFilter(), [this](std::vector<midikraft::PatchHolder> patches) {
		ExportDialog::showExportDialog(this, [this, patches](midikraft::Librarian::ExportParameters params) {
			librarian_.saveSysexPatchesToDisk(params, patches);
		});
	});
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
	loadPage(0, -1, currentFilter(), [this](std::vector<midikraft::PatchHolder> patches) {
		updateLastPath();
		FileChooser pifChooser("Please enter the name of the Patch Interchange Format file to create...", File(lastPathForPIF_), "*.json");
		if (pifChooser.browseForFileToSave(true)) {
			midikraft::PatchInterchangeFormat::save(patches, pifChooser.getResult().getFullPathName().toStdString());
			lastPathForPIF_ = pifChooser.getResult().getFullPathName().toStdString();
			Settings::instance().set("lastPatchInterchangePath", lastPathForPIF_);
		}
	});
}

void PatchView::mergeNewPatches(std::vector<midikraft::PatchHolder> patchesLoaded) {
	MergeManyPatchFiles backgroundThread(database_, patchesLoaded, [this](std::vector<midikraft::PatchHolder> outNewPatches) {
		// Back to UI thread
		MessageManager::callAsync([this, outNewPatches]() {
			if (outNewPatches.size() > 0) {
				patchListTree_.refreshAllImports();
				// Select this import
				auto info = outNewPatches[0].sourceInfo(); //TODO this will break should I change the logic in the PatchDatabase, this is a mere convention
				if (info) {
					auto name = UIModel::currentSynth()->getName();
					if (midikraft::SourceInfo::isEditBufferImport(info)) {
						patchListTree_.selectItemByPath({ "allpatches", "library-" + name, "imports-" + name, "EditBufferImport" });
					}
					else {
						patchListTree_.selectItemByPath({ "allpatches", "library-" + name, "imports-" + name, info->md5(UIModel::currentSynth())});
					}
				}
			}
		});
	});
	backgroundThread.runThread();
}

void PatchView::selectPatch(midikraft::PatchHolder &patch, bool alsoSendToSynth)
{
	auto layers = midikraft::Capability::hasCapability<midikraft::LayeredPatchCapability>(patch.patch());
	// Always refresh the compare target, you just expect it after you clicked it!
	compareTarget_ = UIModel::currentPatch(); // Previous patch is the one we will compare with
	// It could be that we clicked on the patch that is already loaded?
	if (patch.patch() != UIModel::currentPatch().patch() || !layers) {
		//SimpleLogger::instance()->postMessage("Selected patch " + patch.patch()->patchName());
		//logger_->postMessage(patch.patch()->patchToTextRaw(true));

		UIModel::instance()->currentPatch_.changeCurrentPatch(patch);
		currentLayer_ = 0;

		if (alsoSendToSynth) {
			// Send out to Synth
			patch.synth()->sendDataFileToSynth(patch.patch(), nullptr);
		}
	}
	else {
		if (alsoSendToSynth) {
			// Toggle through the layers, if the patch is a layered patch...
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
					totalSize = std::accumulate(allMessages.cbegin(), allMessages.cend(), totalSize, [](int acc, MidiMessage const& m) { return m.getRawDataSize() + acc; });
					SimpleLogger::instance()->postMessage((boost::format("Sending %d messages, total size %d bytes") % allMessages.size() % totalSize).str());
					patch.synth()->sendBlockOfMessagesToSynth(location->midiOutput(), allMessages);
				}
				else {
					jassertfalse;
				}
			}
		}
	}
}

