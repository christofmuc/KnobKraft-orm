/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "PatchView.h"

#include "OrmViews.h"

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
#include "StoredPatchNameCapability.h"
#include "ScriptedQuery.h"
#include "LibrarianProgressWindow.h"

#include "GenericAdaptation.h" //TODO For the Python runtime. That should probably go to its own place, as Python now is used for more than the GenericAdaptation

#include <fmt/format.h>
#include "PatchInterchangeFormat.h"
#include "Settings.h"
#include "ReceiveManualDumpWindow.h"
#include "ExportDialog.h"
#include "BulkRenameDialog.h"
#include "SynthBank.h"

#include "LayoutConstants.h"

#include <spdlog/spdlog.h>
#include "SpdLogJuce.h"

const char *kAllPatchesFilter = "All patches";

PatchView::PatchView()
	: patchListTree_()
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
		UIModel::instance()->currentPatch_.changeCurrentPatch(patch);
	};

	patchButtons_ = std::make_unique<PatchButtonPanel>([this](midikraft::PatchHolder& patch) {
		if (UIModel::currentSynth()) {
			UIModel::instance()->currentPatch_.changeCurrentPatch(patch);
		}
	});

	patchSearch_ = std::make_unique<PatchSearchComponent>(this, patchButtons_.get(), OrmViews::instance().patchDatabase());

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
		patchSearch_->setBounds(area.removeFromLeft(area.getWidth()));
	};
	centerBox->addAndMakeVisible(*patchSearch_);

	splitters_ = std::make_unique<SplitteredComponent>("PatchViewSplitter",
		SplitteredEntry{ box, 15, 5, 40 },
		SplitteredEntry{ centerBox, 85, 40, 90 },
		true);
	addAndMakeVisible(splitters_.get());

	addAndMakeVisible(recycleBin_);

	patchButtons_->setPatchLoader([this](int skip, int limit, std::function<void(std::vector< midikraft::PatchHolder>)> callback) {
		loadPage(skip, limit, currentFilter(), callback);
	});

	// Register for updates
	UIModel::instance()->currentPatch_.addChangeListener(this);
}

PatchView::~PatchView()
{
	UIModel::instance()->currentPatch_.removeChangeListener(this);
	BulkRenameDialog::release();
}

void PatchView::changeListenerCallback(ChangeBroadcaster* source)
{
	if (dynamic_cast<CurrentPatch *>(source)) {
		//currentPatchDisplay_->setCurrentPatch(std::make_shared<midikraft::PatchHolder>(UIModel::currentPatch()));
	}
}

std::vector<CategoryButtons::Category> PatchView::predefinedCategories()
{
	std::vector<CategoryButtons::Category> result;
	for (const auto& c : OrmViews::instance().patchDatabase().getCategories()) {
		if (c.def()->isActive) {
			result.emplace_back(c.category(), c.color());
		}
	}
	return result;
}

void PatchView::retrieveFirstPageFromDatabase() {
	// First, we need to find out how many patches there are (for the paging control)
	int total = OrmViews::instance().patchDatabase().getPatchesCount(currentFilter());
	patchButtons_->setTotalCount(total);
	patchButtons_->refresh(true); // This kicks of loading the first page
}

std::shared_ptr<midikraft::PatchList>  PatchView::retrieveListFromDatabase(midikraft::ListInfo const& info)
{
	if (info.id.empty()) {
		return nullptr;
	}

	std::map<std::string, std::weak_ptr<midikraft::Synth>> synths;
	for (auto synth : UIModel::instance()->synthList_.activeSynths()) {
		if (auto s = std::dynamic_pointer_cast<midikraft::Synth>(synth)) {
			synths.emplace(s->getName(), s);
		}
	}
	return OrmViews::instance().patchDatabase().getPatchList(info, synths);
}

void PatchView::hideCurrentPatch()
{
	selectNextPatch();
	//currentPatchDisplay_->toggleHide();
}

void PatchView::favoriteCurrentPatch()
{
	//currentPatchDisplay_->toggleFavorite();
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
	OrmViews::instance().patchDatabase().getPatchesAsync(filter, [this, callback](midikraft::PatchFilter const filter, std::vector<midikraft::PatchHolder> const &newPatches) {
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
		splitters_->setBounds(area.withTrimmedBottom(LAYOUT_INSET_NORMAL));
	}
	/*else {
		// Portrait
		auto topRow = area.removeFromTop(100);
		buttonStrip_.setBounds(area.removeFromBottom(60).reduced(8));
		//currentPatchDisplay_->setBounds(topRow);
		splitters_->setBounds(area);
	}*/
}

void PatchView::saveCurrentPatchCategories() {
	/*if (currentPatchDisplay_->getCurrentPatch()->patch()) {
		OrmViews::instance().patchDatabase().putPatch(*currentPatchDisplay_->getCurrentPatch());
		patchButtons_->refresh(false);
	}*/
}

void PatchView::loadSynthBankFromDatabase(std::shared_ptr<midikraft::Synth> synth, MidiBankNumber bank, std::string const& bankId)
{
	loadPage(0, -1, bankFilter(synth, bankId), [this, synth, bank, bankId](std::vector<midikraft::PatchHolder> patches) {
		spdlog::info("Bank of {} patches retrieved from database", patches.size());

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
		auto fullInfo = OrmViews::instance().patchDatabase().getPatchList(info, synths);
		if (fullInfo) {
			auto bankList = std::dynamic_pointer_cast<midikraft::SynthBank>(fullInfo);
			if (bankList) {
				UIModel::instance()->synthBank.setSynthBank(bankList);
				//TODO could be transported via UIModel?
				//synthBank_->setBank(bankList, PatchButtonInfo::DefaultDisplay);
			} 
		}
		else {
			spdlog::error("Program Error: Invalid synth bank, not stored in database. Can't load into panel");
			return;
		}
	});
}

void PatchView::retrieveBankFromSynth(std::shared_ptr<midikraft::Synth> synth, MidiBankNumber bank, std::function<void()> finishedHandler)
{
	auto device = std::dynamic_pointer_cast<midikraft::DiscoverableDevice>(synth);
	auto location = midikraft::Capability::hasCapability<midikraft::MidiLocationCapability>(synth);
	if (location) {
		if (location->channel().isValid() && device->wasDetected()) {
			// We can offer to download the bank from the synth, or rather just do it!
			auto progressWindow = std::make_shared<LibrarianProgressWindow>(OrmViews::instance().librarian());
			if (synth /*&& device->wasDetected()*/) {
				midikraft::MidiController::instance()->enableMidiInput(location->midiInput());
				progressWindow->launchThread();
				progressWindow->setMessage(fmt::format("Importing {} from {}...", midikraft::SynthBank::friendlyBankName(synth, bank), synth->getName()));
				OrmViews::instance().librarian().startDownloadingAllPatches(
					midikraft::MidiController::instance()->getMidiOutput(location->midiOutput()),
					synth,
					bank,
					progressWindow.get(), [this, progressWindow, finishedHandler, synth, bank](std::vector<midikraft::PatchHolder> patchesLoaded) {
						progressWindow->signalThreadShouldExit();
						MessageManager::callAsync([this, patchesLoaded, finishedHandler, synth, bank]() {
							spdlog::info("Retrieved {} patches from synth", patchesLoaded.size());
							// First make sure all patches are stored in the database
							auto enhanced = autoCategorize(patchesLoaded);
							mergeNewPatches(enhanced); //This is actually async!, should be reflected in the name. Maybe I should open a progress dialog here?
							// Then store the list of them in the database
							auto retrievedBank = std::make_shared<midikraft::ActiveSynthBank>(synth, bank, juce::Time::getCurrentTime());
							retrievedBank->setPatches(patchesLoaded);
							OrmViews::instance().patchDatabase().putPatchList(retrievedBank);
							// We need to mark something as "active in synth" together with position in the patch_in_list table, so we now when we can program change to the patch
							// instead of sending the sysex
							patchListTree_.refreshAllUserLists();
							loadSynthBankFromDatabase(synth, bank, midikraft::ActiveSynthBank::makeId(synth, bank));
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
		spdlog::error("Invalid operation - cannot retrieve bank from synth that has no MIDI connectivity implemented");
	}
}

void PatchView::sendBankToSynth(std::shared_ptr<midikraft::SynthBank> bankToSend, std::function<void()> finishedHandler)
{
	if (!bankToSend) return;

	auto device = std::dynamic_pointer_cast<midikraft::DiscoverableDevice>(bankToSend->synth());
	auto location = midikraft::Capability::hasCapability<midikraft::MidiLocationCapability>(bankToSend->synth());
	if (location) {
		if (location->channel().isValid() && device->wasDetected()) {
			auto progressWindow = std::make_shared<LibrarianProgressWindow>(OrmViews::instance().librarian());
			if (bankToSend->synth() /*&& device->wasDetected()*/) {
				midikraft::MidiController::instance()->enableMidiInput(location->midiInput());
				progressWindow->launchThread();
				OrmViews::instance().librarian().sendBankToSynth(*bankToSend, false, progressWindow.get(), [this, bankToSend, finishedHandler](bool completed) {
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
		spdlog::error("Invalid operation - cannot send bank to synth that has no MIDI connectivity implemented");
	}
}

void PatchView::setSynthBankFilter(std::shared_ptr<midikraft::Synth> synth, MidiBankNumber bank) {
	auto bankId = midikraft::ActiveSynthBank::makeId(synth, bank);
	// Check if this synth bank has ever been loaded
	std::map<std::string, std::weak_ptr<midikraft::Synth>> synths;
	synths[synth->getName()] = synth;
	if (OrmViews::instance().patchDatabase().doesListExist(bankId)) {
		// It does, so we can safely load and display it
		loadSynthBankFromDatabase(synth, bank, bankId);
	}
	else {
		// No, first time ever - offer the user to download from the synth if connected
		retrieveBankFromSynth(synth, bank, [this, synth, bank]() {
			// After it has been loaded successfully, make sure to select it in the tree 
			std::string synthName = synth->getName();
			patchListTree_.selectItemByPath({ "allpatches", "library-" + synthName, "banks-" + synthName});
		});
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
				OrmViews::instance().patchDatabase().deletePatches(infos["synth"], { infos["md5"] });
				spdlog::info("Deleted patch {} from database", patchName);
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
			OrmViews::instance().patchDatabase().removePatchFromList(list_id, infos["synth"], infos["md5"], infos["order_num"]);
			spdlog::info("Removed patch {} from list {}", patch_name,  list_name);
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
				OrmViews::instance().patchDatabase().deletePatchlist(midikraft::ListInfo{ list_id, list_name });
				spdlog::info("Deleted list {}", list_name);
				if (listFilterID_ == list_id) {
				}
				patchListTree_.refreshAllUserLists();
			}
			return;
		}
	}
	spdlog::error("Program error - unknow drop type dropped on recycle bin!");
}

void PatchView::retrievePatches() {
	auto activeSynth = UIModel::instance()->currentSynth_.smartSynth();
	auto device = std::dynamic_pointer_cast<midikraft::DiscoverableDevice>(activeSynth);
	auto midiLocation = midikraft::Capability::hasCapability<midikraft::MidiLocationCapability>(activeSynth);
	std::shared_ptr<ProgressHandlerWindow> progressWindow = std::make_shared<LibrarianProgressWindow>(OrmViews::instance().librarian());
	if (activeSynth /*&& device->wasDetected()*/) {
		midikraft::MidiController::instance()->enableMidiInput(midiLocation->midiInput());
		importDialog_ = std::make_unique<ImportFromSynthDialog>(activeSynth,
			[this, progressWindow, activeSynth, midiLocation](std::vector<MidiBankNumber> bankNo) {
			if (!bankNo.empty()) {
				progressWindow->launchThread();
				OrmViews::instance().librarian().startDownloadingAllPatches(
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
	std::vector<midikraft::PatchHolder> result;
	for (auto p : patches) {
		p.autoCategorizeAgain(OrmViews::instance().automaticCategories());
		result.push_back(p);
	}
	return result;
}


void PatchView::retrieveEditBuffer()
{
	auto activeSynth = UIModel::instance()->currentSynth_.smartSynth();
	auto midiLocation = midikraft::Capability::hasCapability<midikraft::MidiLocationCapability>(activeSynth);
	if (activeSynth && midiLocation) {
		OrmViews::instance().librarian().downloadEditBuffer(midikraft::MidiController::instance()->getMidiOutput(midiLocation->midiOutput()),
			activeSynth,
			nullptr,
			[this](std::vector<midikraft::PatchHolder> patchesLoaded) {
			// There should only be one edit buffer, just check that this is true here
			jassert(patchesLoaded.size() == 1);

			if (patchesLoaded.size() == 1) {
				spdlog::info("Current edit buffer from synth is patch '{}'", patchesLoaded[0].name());
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

void PatchView::bulkRenamePatches()
{
	loadPage(0, 512, currentFilter(), [this](std::vector<midikraft::PatchHolder> patches) {
		BulkRenameDialog::show(patches, this, [this](std::vector<midikraft::PatchHolder> renamed) {
			std::vector<midikraft::PatchHolder> newPatches_;
			size_t numChanged = OrmViews::instance().patchDatabase().mergePatchesIntoDatabase(renamed, newPatches_, nullptr, midikraft::PatchDatabase::UPDATE_NAME);
			spdlog::info("Renamed {} patches in the database!", numChanged);
			retrieveFirstPageFromDatabase();
			});
		});
}

void PatchView::deletePatches()
{
	int totalAffected = totalNumberOfPatches();
	if (AlertWindow::showOkCancelBox(AlertWindow::QuestionIcon, fmt::format("Delete all {} patches matching current filter", totalAffected),
		fmt::format("Warning, there is no undo operation. Do you really want to delete the {} patches matching the current filter?\n\n"
			"They will be gone forever, unless you use a backup!", totalAffected))) {
		if (AlertWindow::showOkCancelBox(AlertWindow::WarningIcon, "Do you know what you are doing?",
			"Are you sure?", "Yes", "No")) {
			int deleted = OrmViews::instance().patchDatabase().deletePatches(currentFilter());
			AlertWindow::showMessageBox(AlertWindow::InfoIcon, "Patches deleted", fmt::format("{} patches deleted from database", deleted));
			UIModel::instance()->importListChanged_.sendChangeMessage();
			retrieveFirstPageFromDatabase();
		}
	}
}

void PatchView::reindexPatches() {
	// We do reindex all patches of the currently selected synth. It does not make sense to reindex less than that.
	auto currentSynth = UIModel::instance()->currentSynth_.smartSynth();
	if (!currentSynth) return;
	midikraft::PatchFilter filter({ currentSynth });
	filter.turnOnAll(); // Make sure we also reindex hidden entries

	int totalAffected = OrmViews::instance().patchDatabase().getPatchesCount(filter);
	if (AlertWindow::showOkCancelBox(AlertWindow::QuestionIcon, fmt::format("Do you want to reindex all {} patches for synth {}?", totalAffected, currentSynth->getName()),
		fmt::format("This will reindex the {} patches with the current fingerprinting algorithm.\n\n"
			"Hopefully this will get rid of duplicates properly, but if there are duplicates under multiple names you'll end up with a somewhat random result which name is chosen for the de-duplicated patch.\n",
			totalAffected))) {
		std::string backupName = OrmViews::instance().patchDatabase().makeDatabaseBackup("-before-reindexing");
		spdlog::info("Created database backup at {}", backupName);
		int countAfterReindexing = OrmViews::instance().patchDatabase().reindexPatches(filter);
		if (countAfterReindexing != -1) {
			// No error, display user info
			if (totalAffected > countAfterReindexing) {
				AlertWindow::showMessageBox(AlertWindow::InfoIcon, "Reindexing patches successful",
					fmt::format("The reindexing reduced the number of patches from {} to {} due to deduplication.",totalAffected, countAfterReindexing));
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
	return OrmViews::instance().patchDatabase().getPatchesCount(currentFilter());
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
	midikraft::PatchFilter filter({ synth });
	filter.turnOnAll();

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
			spdlog::warn("No patches contained in data, nothing to upload.");
		}
		else {
			auto numberNew = database_.mergePatchesIntoDatabase(patchesLoaded_, outNewPatches, this, midikraft::PatchDatabase::UPDATE_NAME | midikraft::PatchDatabase::UPDATE_CATEGORIES | midikraft::PatchDatabase::UPDATE_FAVORITE);
			if (numberNew > 0) {
				spdlog::info("Retrieved {} new or changed patches from the synth, uploaded to database", numberNew);
				finished_(outNewPatches);
			}
			else {
				spdlog::info("All patches already known to database");
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
			auto patches = OrmViews::instance().librarian().loadSysexPatchesManualDump(synthToReceiveFrom, messagesReceived, OrmViews::instance().automaticCategories());
			if (patches.size() > 0) {
				auto enhanced = autoCategorize(patches);
				mergeNewPatches(enhanced);
			}
		}
	}
}

void PatchView::loadPatches() {
	if (UIModel::currentSynth()) {
		auto synth = UIModel::instance()->currentSynth_.smartSynth();
		auto patches = OrmViews::instance().librarian().loadSysexPatchesFromDisk(synth, OrmViews::instance().automaticCategories());
		if (patches.size() > 0) {
			// If the synth does not offer stored patch names, the names of these patches will be useless defaults only.
			// Open the new bulk rename dialog to allow the user to fix it immediately.
			if (midikraft::Capability::hasCapability<midikraft::StoredPatchNameCapability>(patches[0].patch())) {
				auto enhanced = autoCategorize(patches);
				mergeNewPatches(enhanced);
			}
			else {
				BulkRenameDialog::show(patches, this, [this](std::vector<midikraft::PatchHolder> renamedPatches) {
					auto enhanced = autoCategorize(renamedPatches);
					mergeNewPatches(enhanced);
				});
			}
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
					spdlog::info("Loaded {} additional patches from file {}", numberNew, pip.getFullPathName());
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
	BulkImportPIP bulk(directory, OrmViews::instance().patchDatabase(), OrmViews::instance().automaticCategories());

	bulk.runThread();

	retrieveFirstPageFromDatabase();
}

void PatchView::exportPatches()
{
	loadPage(0, -1, currentFilter(), [this](std::vector<midikraft::PatchHolder> patches) {
		ExportDialog::showExportDialog(this, [this, patches](midikraft::Librarian::ExportParameters params) {
			OrmViews::instance().librarian().saveSysexPatchesToDisk(params, patches);
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
	MergeManyPatchFiles backgroundThread(OrmViews::instance().patchDatabase(), patchesLoaded, [this](std::vector<midikraft::PatchHolder> outNewPatches) {
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


