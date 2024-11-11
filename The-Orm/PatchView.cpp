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
#include "StoredPatchNameCapability.h"
#include "ScriptedQuery.h"
#include "LibrarianProgressWindow.h"

#include "GenericAdaptation.h" //TODO For the Python runtime. That should probably go to its own place, as Python now is used for more than the GenericAdaptation

#include "I18NHelper.h"

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

#include <random>
#include <algorithm>

const char *kAllPatchesFilter = "All patches";

PatchView::PatchView(midikraft::PatchDatabase &database, std::vector<midikraft::SynthHolder> const &synths) :
         patchListTree_(database, synths)
        , rightSideTab_(juce::TabbedButtonBar::TabsAtTop)
        , librarian_(synths)
        , synths_(synths)
        , database_(database)
{
	patchListTree_.onSynthBankSelected = [this](std::shared_ptr<midikraft::Synth> synth, MidiBankNumber bank) {
		setSynthBankFilter(synth, bank);
		showBank();
	};
	patchListTree_.onImportListSelected = [this](String id) {
		setImportListFilter(id);
	};
	patchListTree_.onUserBankSelected = [this](std::shared_ptr<midikraft::Synth> synth, String id) {
		setUserBankFilter(synth, id.toStdString());
		showBank();
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
	patchListTree_.onPatchListFill = [this](std::shared_ptr<midikraft::PatchList> list, CreateListDialog::TFillParameters parameters, std::function<void()> finishedCallback) {
		fillList(list, parameters, finishedCallback);
	};

	patchButtons_ = std::make_unique<PatchButtonPanel>([this](midikraft::PatchHolder& patch) {
		if (UIModel::currentSynth()) {
			selectPatch(patch, true);
		}
	});

	currentPatchDisplay_ = std::make_unique<CurrentPatchDisplay>(database_, predefinedCategories(),
		[this](std::shared_ptr<midikraft::PatchHolder> favoritePatch) {
		database_.putPatch(*favoritePatch);
		int total = getTotalCount();
		patchButtons_->setTotalCount(total, false);
		patchButtons_->refresh(true);
		synthBank_->refreshPatch(favoritePatch);
	}
	);
	currentPatchDisplay_->onCurrentPatchClicked = [this](std::shared_ptr<midikraft::PatchHolder> patch) {
		if (patch) {
			selectPatch(*patch, true);
		}
	};

	synthBank_ = std::make_unique<SynthBankPanel>(database_, this);
	patchHistory_ = std::make_unique<PatchHistoryPanel>(this);

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
	recycleBin_.onClicked = []() {
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
	
	/*auto centerBox = new LambdaLayoutBox();
	centerBox->onResized = [this](Component* box) {
		auto area = box->getLocalBounds();
		patchSearch_->setBounds(area.removeFromLeft(area.getWidth() / 4 * 3));
		synthBank_->setBounds(area.reduced(LAYOUT_INSET_NORMAL));
	};
	centerBox->addAndMakeVisible(*synthBank_);
	centerBox->addAndMakeVisible(*patchSearch_);*/

	addAndMakeVisible(rightSideTab_);
	rightSideTab_.addTab("Current Patch", Colours::black, currentPatchDisplay_.get(), false);
	rightSideTab_.addTab("Synth Bank", Colours::black, synthBank_.get(), false);
	rightSideTab_.addTab("Recent Patches", Colours::black, patchHistory_.get(), false);

	splitters_ = std::make_unique<SplitteredComponent>("PatchViewSplitter",
		SplitteredEntry{ box, 15, 5, 40 },
		SplitteredEntry{ patchSearch_.get(), 50, 40, 90},
		SplitteredEntry{ &rightSideTab_, 15, 5, 40}, true);
	addAndMakeVisible(splitters_.get());

	addAndMakeVisible(recycleBin_);

	patchButtons_->setPatchLoader([this](int skip, int limit, std::function<void(std::vector< midikraft::PatchHolder>)> callback) {
		loadPage(skip, limit, currentFilter(), callback);
	});

	patchButtons_->setButtonSendModes({ "program change", "edit buffer", "automatic"});

	// Register for updates
	UIModel::instance()->currentPatch_.addChangeListener(this);
}

PatchView::~PatchView()
{
	UIModel::instance()->currentPatch_.removeChangeListener(this);
	BulkRenameDialog::release();
}

void PatchView::showBank() {
	rightSideTab_.setCurrentTabIndex(1, true);
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

int PatchView::getTotalCount() {
	return database_.getPatchesCount(currentFilter());
}

void PatchView::retrieveFirstPageFromDatabase() {
	// First, we need to find out how many patches there are (for the paging control)
	int total = getTotalCount();
	patchButtons_->setTotalCount(total, true);
	patchButtons_->refresh(true); // This kicks of loading the first page
	Data::instance().getEphemeral().setProperty(EPROPERTY_LIBRARY_PATCH_LIST, juce::Uuid().toString(), nullptr);
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
        ignoreUnused(filter);
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

void PatchView::showPatchDiffDialog() {
	if (!compareTarget_.patch() || !UIModel::currentPatch().patch()) {
		// Shouldn't have come here
		return;
	}

	if (compareTarget_.synth()->getName() != UIModel::currentPatch().synth()->getName()) {
		// Should have come either
		spdlog::warn("Can't compare patch {} of synth {} with patch {} of synth {}",
			UIModel::currentPatch().name(), UIModel::currentPatch().synth()->getName(),
			compareTarget_.name(), compareTarget_.synth()->getName());
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
		auto fullInfo = database_.getPatchList(info, synths);
		if (fullInfo) {
			auto bankList = std::dynamic_pointer_cast<midikraft::SynthBank>(fullInfo);
			if (bankList) {
				synthBank_->setBank(bankList, PatchButtonInfo::DefaultDisplay);
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
			auto progressWindow = std::make_shared<LibrarianProgressWindow>(librarian_, "Import patches from Synth");
			if (synth /*&& device->wasDetected()*/) {
				midikraft::MidiController::instance()->enableMidiInput(location->midiInput());
				progressWindow->launchThread();
				progressWindow->setMessage(fmt::format("Importing {} from {}...", midikraft::SynthBank::friendlyBankName(synth, bank), synth->getName()));
				librarian_.startDownloadingAllPatches(
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
							database_.putPatchList(retrievedBank);
							// We need to mark something as "active in synth" together with position in the patch_in_list table, so we now when we can program change to the patch
							// instead of sending the sysex
							patchListTree_.refreshAllUserLists([this, synth, bank, finishedHandler]() {
								loadSynthBankFromDatabase(synth, bank, midikraft::ActiveSynthBank::makeId(synth, bank));
								if (finishedHandler) {
									finishedHandler();
								}
								});
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

void PatchView::sendBankToSynth(std::shared_ptr<midikraft::SynthBank> bankToSend, bool ignoreDirty, std::function<void()> finishedHandler)
{
	if (!bankToSend) return;

	auto device = std::dynamic_pointer_cast<midikraft::DiscoverableDevice>(bankToSend->synth());
	auto location = midikraft::Capability::hasCapability<midikraft::MidiLocationCapability>(bankToSend->synth());
	if (location) {
		if (location->channel().isValid() && device->wasDetected()) {
			auto progressWindow = std::make_shared<LibrarianProgressWindow>(librarian_, "Sending bank to Synth");
			progressWindow->setMessage("Starting send");
			if (bankToSend->synth() /*&& device->wasDetected()*/) {
				midikraft::MidiController::instance()->enableMidiInput(location->midiInput());
				progressWindow->launchThread();
				librarian_.sendBankToSynth(*bankToSend, ignoreDirty, progressWindow.get(), [bankToSend, finishedHandler, progressWindow](bool completed) {
					progressWindow->signalThreadShouldExit();
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
	if (database_.doesListExist(bankId)) {
		// It does, so we can safely load and display it
		loadSynthBankFromDatabase(synth, bank, bankId);
	}
	else {
		// No, first time ever - offer the user to download from the synth if connected
		retrieveBankFromSynth(synth, bank, [this, synth]() {
			// After it has been loaded successfully, make sure to select it in the tree 
			std::string synthName = synth->getName();
			patchListTree_.selectItemByPath({ "allpatches", "library-" + synthName, "banks-" + synthName});
		});
	}
}

void PatchView::setUserBankFilter(std::shared_ptr<midikraft::Synth> synth, std::string const& listId) {
	if (database_.doesListExist(listId)) {
		// It does, so we can safely load and display it
		loadSynthBankFromDatabase(synth, MidiBankNumber::invalid(), listId);
	}
	else {
		jassertfalse;
	}
}

void PatchView::copyBankPatchNamesToClipboard() {
    synthBank_->copyPatchNamesToClipboard();
}



void PatchView::setImportListFilter(String filter)
{
	listFilterID_ = filter.toStdString();
	retrieveFirstPageFromDatabase();
}

void PatchView::setUserListFilter(String filter)
{
	listFilterID_ = filter.toStdString();
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
				auto [deleted, hidden] = database_.deletePatches(infos["synth"], { infos["md5"] });
				if (deleted > 0) {
					spdlog::info("Deleted patch {} from database", patchName);
				}
				else if (hidden > 0 ) {
					spdlog::warn("Could not delete patch {} from database as it is referred to be at least one bank definition. Removed it from user lists and set it to hidden instead!", patchName);
				} 
				else {
					spdlog::error("Program error, could not delete patch");
				}
				patchListTree_.refreshAllUserLists([]() {});
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
			spdlog::info("Removed patch {} from list {}", patch_name,  list_name);
			patchListTree_.refreshChildrenOfListId(list_id, []() {});
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
				spdlog::info("Deleted list {}", list_name);
				if (listFilterID_ == list_id) {
				}
				patchListTree_.refreshParentOfListId(list_id, []() {});
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
	std::shared_ptr<ProgressHandlerWindow> progressWindow = std::make_shared<LibrarianProgressWindow>(librarian_, "Import patches from Synth");
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
	std::vector<midikraft::PatchHolder> result;
	for (auto p : patches) {
		p.autoCategorizeAgain(database_.getCategorizer());
		result.push_back(p);
	}
	return result;
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
			database_.mergePatchesIntoDatabase(renamed, newPatches_, nullptr, midikraft::PatchDatabase::UPDATE_NAME);
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
			int deleted = database_.deletePatches(currentFilter());
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

	int totalAffected = database_.getPatchesCount(filter);
	if (AlertWindow::showOkCancelBox(AlertWindow::QuestionIcon, fmt::format("Do you want to reindex all {} patches for synth {}?", totalAffected, currentSynth->getName()),
		fmt::format("This will reindex the {} patches with the current fingerprinting algorithm.\n\n"
			"Hopefully this will get rid of duplicates properly, but if there are duplicates under multiple names you'll end up with a somewhat random result which name is chosen for the de-duplicated patch.\n",
			totalAffected))) {
		std::string backupName = database_.makeDatabaseBackup("-before-reindexing");
		spdlog::info("Created database backup at {}", backupName);
		int countAfterReindexing = database_.reindexPatches(filter);
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
	return database_.getPatchesCount(currentFilter());
}

void PatchView::selectFirstPatch()
{
	patchButtons_->selectFirst();
}

midikraft::PatchFilter PatchView::currentFilter()
{
	auto filter = patchSearch_->getFilter();
	filter.listID = listFilterID_;
	if (!filter.listID.empty()) {
		filter.orderBy = midikraft::PatchOrdering::Order_by_Place_in_List;
	}
	return filter;
}

midikraft::PatchFilter PatchView::bankFilter(std::shared_ptr<midikraft::Synth> synth, std::string const& listID)
{
	// We want to load all patches for this synth that are in the bank list given
	midikraft::PatchFilter filter({ synth });
	filter.turnOnAll();

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
		auto synth = UIModel::instance()->currentSynth_.smartSynth();
		auto patches = librarian_.loadSysexPatchesFromDisk(synth, database_.getCategorizer());
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
	BulkImportPIP bulk(directory, database_, database_.getCategorizer());

	bulk.runThread();

	retrieveFirstPageFromDatabase();
}

void PatchView::exportPatches()
{
	loadPage(0, -1, currentFilter(), [this](std::vector<midikraft::PatchHolder> patches) {
		ExportDialog::showExportDialog(this, "Export patches", [this, patches](midikraft::Librarian::ExportParameters params) {
			librarian_.saveSysexPatchesToDisk(params, patches);
		});
	});
}

void PatchView::exportBank()
{
	auto currentBank = synthBank_->getCurrentSynthBank();
	if (currentBank) {
		auto patches = currentBank->patches();
		ExportDialog::showExportDialog(this, "Export bank", [this, patches](midikraft::Librarian::ExportParameters params) {
			librarian_.saveSysexPatchesToDisk(params, patches);
			});
	}
	else {
		AlertWindow::showMessageBox(AlertWindow::InfoIcon, "Nothing to export",  "Please select a bank first!");
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
				patchListTree_.refreshAllImports([outNewPatches, this]() {
					// Select this import
					auto info = outNewPatches[0].sourceInfo(); //TODO this will break should I change the logic in the PatchDatabase, this is a mere convention
					if (info) {
						auto name = UIModel::currentSynth()->getName();
						if (midikraft::SourceInfo::isEditBufferImport(info)) {
							patchListTree_.selectItemByPath({ "allpatches", "library-" + name, "imports-" + name, "EditBufferImport" });
						}
						else {
							patchListTree_.selectItemByPath({ "allpatches", "library-" + name, "imports-" + name, info->md5(UIModel::currentSynth()) });
						}
					}
					});
			}
		});
	});
	backgroundThread.runThread();
}

std::vector<MidiProgramNumber> PatchView::patchIsInSynth(midikraft::PatchHolder& patch) {
	auto alreadyInSynth = database_.getBankPositions(patch.smartSynth(), patch.md5());
	for (auto inSynth : alreadyInSynth) {
		if (inSynth.bank().isValid()) {
			spdlog::debug("Patch is already in synth in bank {} at position {}", inSynth.bank().toZeroBased(), inSynth.toZeroBasedDiscardingBank());
		}
		else
		{
			spdlog::debug("Patch is already in synth in unknown bank at position {}", inSynth.toZeroBasedDiscardingBank());
		}
	}
	if (alreadyInSynth.size() > 1) {
		spdlog::debug("Patch {} is in {} different positions.", patch.name(), alreadyInSynth.size());
	}
	return alreadyInSynth;
}

bool PatchView::isSynthConnected(std::shared_ptr<midikraft::Synth> synth) {
	auto midiLocation = midikraft::Capability::hasCapability<midikraft::MidiLocationCapability>(synth);
	return midiLocation && midiLocation->channel().isValid();
}

std::vector<MidiMessage> PatchView::buildSelectBankAndProgramMessages(MidiProgramNumber program, midikraft::PatchHolder &patch) {
	// Default to the bank of the patch in case the given program number contains no bank
	auto bankNumberToSelect = patch.bankNumber();
	if (program.isBankKnown()) {
		bankNumberToSelect = program.bank();
	}

	std::vector<juce::MidiMessage> selectPatch;
	if (auto bankDescriptors = midikraft::Capability::hasCapability<midikraft::HasBankDescriptorsCapability>(patch.smartSynth())) {
		auto bankSelect = bankDescriptors->bankSelectMessages(bankNumberToSelect);
		std::copy(bankSelect.cbegin(), bankSelect.cend(), std::back_inserter(selectPatch));
	}
	else if (auto banks = midikraft::Capability::hasCapability<midikraft::HasBanksCapability>(patch.smartSynth())) {
		auto bankSelect = banks->bankSelectMessages(bankNumberToSelect);
		std::copy(bankSelect.cbegin(), bankSelect.cend(), std::back_inserter(selectPatch));
	}

	auto midiLocation = midikraft::Capability::hasCapability<midikraft::MidiLocationCapability>(patch.smartSynth());
	if (midiLocation && midiLocation->channel().isValid()) {
		selectPatch.push_back(MidiMessage::programChange(midiLocation->channel().toOneBasedInt(), program.toZeroBasedDiscardingBank()));
		spdlog::info("Sending program change to {} for patch {}: program {} {}."
			, patch.smartSynth()->getName()
			, patch.name()
			, patch.smartSynth()->friendlyProgramAndBankName(bankNumberToSelect, program)
			, program.isBankKnown() ? "[known bank]" : "[bank not known!]");			
		return selectPatch;
	} else {
		spdlog::error("Program error - Synth {} has not been detected, can't build MIDI messages to select bank and program", patch.smartSynth()->getName());
		return {};
	}
}

void PatchView::sendProgramChangeMessagesForPatch(std::shared_ptr<midikraft::MidiLocationCapability> midiLocation,  MidiProgramNumber program, midikraft::PatchHolder &patch) {
	// We can get away with just a bank select and program change, and will try to select the patch directly
		// Build the MIDI messages required to select bank and program
	auto selectPatch = buildSelectBankAndProgramMessages(program, patch);
	if (selectPatch.size() > 0) {
		patch.smartSynth()->sendBlockOfMessagesToSynth(midiLocation->midiOutput(), selectPatch);
	}
	else {
		spdlog::error("Failed to build MIDI bank and program change messages for {}, program error?", patch.smartSynth()->getName());
	}

}

void PatchView::sendPatchAsSysex(midikraft::PatchHolder &patch) {
	// Send out to Synth into edit buffer
	if (patch.patch()) {
		spdlog::info("Sending sysex for patch '{}' to {}", patch.name(), patch.synth()->getName());
		patch.synth()->sendDataFileToSynth(patch.patch(), nullptr);
	}
	else {
		spdlog::debug("Empty patch slot selected, can't send to synth");
	}
}


void PatchView::selectPatch(midikraft::PatchHolder &patch, bool alsoSendToSynth)
{
	auto layers = midikraft::Capability::hasCapability<midikraft::LayeredPatchCapability>(patch.patch());
	// Always refresh the compare target, you just expect it after you clicked it!
	compareTarget_ = UIModel::currentPatch(); // Previous patch is the one we will compare with
	// It could be that we clicked on the patch that is already loaded?
//	if (patch.patch() != UIModel::currentPatch().patch() || !layers) {
		//SimpleLogger::instance()->postMessage("Selected patch " + patch.patch()->patchName());
		//logger_->postMessage(patch.patch()->patchToTextRaw(true));

		UIModel::instance()->currentPatch_.changeCurrentPatch(patch);
		currentLayer_ = 0;

		if (alsoSendToSynth) {
			auto midiLocation = midikraft::Capability::hasCapability<midikraft::MidiLocationCapability>(patch.smartSynth());
			if (isSynthConnected(patch.smartSynth())) {
				UIModel::ensureSynthSpecificPropertyExists(patch.smartSynth()->getName(), PROPERTY_COMBOBOX_SENDMODE, "automatic");
				auto synthSpecificSendMode = UIModel::instance()->getSynthSpecificPropertyAsValue(patch.smartSynth()->getName(), PROPERTY_COMBOBOX_SENDMODE, "automatic").getValue();

				auto alreadyInSynth = patchIsInSynth(patch);
				if (synthSpecificSendMode == "program change") {
					if (alreadyInSynth.size() > 0) {
						sendProgramChangeMessagesForPatch(midiLocation, alreadyInSynth[0], patch);
					}
					else {
						spdlog::info("Patch send mode set to program change, but position of patch in synth is unknown. Try to import the banks of the synth first!");
					}
				}
				else if (synthSpecificSendMode == "edit buffer") {
					sendPatchAsSysex(patch);
				}
				else if (synthSpecificSendMode == "automatic") {
					if (alreadyInSynth.size() > 0) {
						sendProgramChangeMessagesForPatch(midiLocation, alreadyInSynth[0], patch);
					}
					else {
						sendPatchAsSysex(patch);
					}
				} 
				else {
					spdlog::error("Unknown send mode '{}' stored in property, program error?", synthSpecificSendMode.operator juce::String().toStdString());
				}
			}
			else {
				spdlog::info("{} not detected, skipped sending patch {}", patch.smartSynth()->getName(), patch.name());
			}
		}
	/* }
	else {
		if (alsoSendToSynth) {
			// Toggle through the layers, if the patch is a layered patch...
			if (layers) {
				currentLayer_ = (currentLayer_ + 1) % layers->numberOfLayers();
			}
			auto layerSynth = midikraft::Capability::hasCapability<midikraft::LayerCapability>(patch.smartSynth());
			if (layerSynth) {
				spdlog::info("Switching to layer {}", currentLayer_);
				//layerSynth->switchToLayer(currentLayer_);
				auto allMessages = layerSynth->layerToSysex(patch.patch(), 1, 0);
				auto location = midikraft::Capability::hasCapability<midikraft::MidiLocationCapability>(patch.smartSynth());
				if (location) {
					int totalSize = 0;
					totalSize = std::accumulate(allMessages.cbegin(), allMessages.cend(), totalSize, [](int acc, MidiMessage const& m) { return m.getRawDataSize() + acc; });
					spdlog::debug("Sending {} messages, total size {} bytes", allMessages.size(), totalSize);
					patch.synth()->sendBlockOfMessagesToSynth(location->midiOutput(), allMessages);
				}
				else {
					jassertfalse;
				}
			}
		}
	}*/
}

template <typename T>
std::vector<T> getRandomSubset(const std::vector<T>& original, std::size_t subsetSize) {
	// Copy the original vector
	std::vector<T> shuffled = original;

	// If subsetSize is larger than the original vector size, limit it
	if (subsetSize > original.size()) {
		subsetSize = original.size();
	}

	// Create a random engine with a seed based on the current time
	std::random_device rd;
	std::default_random_engine rng(rd());

	// Shuffle the copied vector
	std::shuffle(shuffled.begin(), shuffled.end(), rng);

	// Create a vector to store the subset
	std::vector<T> subset(shuffled.begin(), shuffled.begin() + subsetSize);

	return subset;
}

void PatchView::fillList(std::shared_ptr<midikraft::PatchList> list, CreateListDialog::TFillParameters fillParameters, std::function<void()> finishedCallback) {
	if (fillParameters.fillMode == CreateListDialog::TListFillMode::None) {
		finishedCallback();
	}
	else  {
		auto filter = currentFilter();
		auto synthBank = std::dynamic_pointer_cast<midikraft::SynthBank>(list);
		size_t patchesDesired = fillParameters.number;
		size_t minimumPatches = 0;
		if (synthBank) {
			// This is a synth bank, restrict the filter to deliver only patches for the synth that the bank is for
			filter.synths.clear();
			filter.synths[synthBank->synth()->getName()] = synthBank->synth();
			patchesDesired = synthBank->patchCapacity();
			if (synthBank->bankNumber().bankSize() >= 0) {
				minimumPatches = (size_t) synthBank->bankNumber().bankSize();
			}
			else {
				spdlog::error("Program error: Unknown bank size, can't fill bank with unknown number of patches");
				return;
			}
		}

        if(database_.getPatchesCount(currentFilter()) == 0) {
            spdlog::error("The list can't be filled, there are no patches in the database matching the current filter.");
            return;
        }

        if (fillParameters.fillMode == CreateListDialog::TListFillMode::Top) {
			loadPage(0, (int) patchesDesired, filter, [list, finishedCallback, minimumPatches](std::vector<midikraft::PatchHolder> patches) {
				// Check if we need to extend the patches list to make sure we have enough patches to make a full bank
				while (patches.size() < minimumPatches) {
					patches.push_back(patches.back());
				}
				list->setPatches(patches);
				finishedCallback();
				});
		}
		else if (fillParameters.fillMode == CreateListDialog::TListFillMode::Random) {
			loadPage(0, -1, filter, [list, patchesDesired, minimumPatches, finishedCallback](std::vector<midikraft::PatchHolder> patches) {
				// Check if we need to extend the patches list to make sure we have enough patches to make a full bank
				auto randomPatches = getRandomSubset(patches, patchesDesired);
				while (randomPatches.size() < minimumPatches) {
					randomPatches.push_back(randomPatches.back());
				}
				list->setPatches(randomPatches);
				finishedCallback();
				});
		}
	}
}

