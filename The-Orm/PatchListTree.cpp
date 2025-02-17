/*
   Copyright (c) 2021 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "PatchListTree.h"

#include "CreateListDialog.h"

#include "UIModel.h"
#include "Logger.h"
#include "ColourHelpers.h"
#include "HasBanksCapability.h"

#include <spdlog/spdlog.h>
#include "SpdLogJuce.h"

const std::string kAllPatchesTree("allpatches");
const std::string kLibraryTreePrefix("library-");
const std::string kSynthBanksPrefix("banks-");
const std::string kUserBanksPrefix("stored-banks-");
const std::string kImportsTreePrefix("imports-");
const std::string kUserListsTree("userlists");

void shortenImportNames(std::vector<midikraft::ImportInfo>& imports) {
	for (auto& import : imports) {
		if (import.name.rfind("Imported from file") == 0) {
			import.name = import.name.substr(19);
		}
		else if (import.name.rfind("Imported from synth") == 0) {
			import.name = import.name.substr(20);
		}
	}
}

template <class T>
std::vector<T> sortLists(std::vector<T> const& lists, std::function<std::string(T const&)> key) {
	// We use the JUCE natural language sort, but for that we need to build a StringArray of the names first...
	std::map<std::string, T> byName;
	StringArray names;
	for (auto const& list : lists) {
		byName[key(list)] = list;
		names.add(key(list));
	}
	names.sortNatural();
	std::vector<T> result;
	for (auto const& name : names) {
		result.push_back(byName[name.toStdString()]);
	}
	return result;
}

class ImportNameListener : public Value::Listener {
public:
	ImportNameListener(midikraft::PatchDatabase& db, std::string synthName, std::string importID) : db_(db), synthName_(synthName), importID_(importID) {
	}

	virtual void valueChanged(Value& value) {
		spdlog::info("Changed name of import to {}", value.getValue().toString());
		String newValue = value.getValue();
		db_.renameImport(synthName_, importID_, newValue.toStdString());
	}

private:
	midikraft::PatchDatabase& db_;
	std::string synthName_;
	std::string importID_;
};

PatchListTree::PatchListTree(midikraft::PatchDatabase& db, std::vector<midikraft::SynthHolder> const& synths)
	: db_(db)
{
	treeView_ = std::make_unique<TreeView>();
	treeView_->setOpenCloseButtonsVisible(true);
	addAndMakeVisible(*treeView_);

	// Build data structure to load patch lists
	for (auto synth : synths) {
		synths_[synth.getName()] = synth.synth();
	}

	allPatchesItem_ = new TreeViewNode("All patches", kAllPatchesTree);
	allPatchesItem_->onSelected = [this](String id) {
        juce::ignoreUnused(id);
		UIModel::instance()->multiMode_.setMultiSynthMode(true);
		if (onImportListSelected)
			onImportListSelected("");
	};
	allPatchesItem_->onGenerateChildren = [this]() {
		std::vector<TreeViewItem*> result;
		for (auto activeSynth : UIModel::instance()->synthList_.activeSynths()) {
			std::string synthName = activeSynth->getName();
			auto synthLibrary = new TreeViewNode(synthName, kLibraryTreePrefix + synthName);
			synthLibrary->onGenerateChildren = [this, activeSynth]() {
				return std::vector<TreeViewItem*>({
					newTreeViewItemForSynthBanks(activeSynth)
					, newTreeViewItemForStoredBanks(activeSynth)
					, newTreeViewItemForImports(activeSynth) });
			};
			synthLibrary->onSelected = [this, synthName](String id) {
                juce::ignoreUnused(id);
				UIModel::instance()->currentSynth_.changeCurrentSynth(UIModel::instance()->synthList_.synthByName(synthName).synth());
				UIModel::instance()->multiMode_.setMultiSynthMode(false);
				if (onImportListSelected)
					onImportListSelected("");
			};

			result.push_back(synthLibrary);
		}
		return result;
	};

	userListsItem_ = new TreeViewNode("User lists", kUserListsTree);
	userListsItem_->onGenerateChildren = [this]() {
		std::vector<TreeViewItem*> result;
		auto userLists = db_.allPatchLists();
		userLists = sortLists<midikraft::ListInfo>(userLists, [](const midikraft::ListInfo& info) { return info.name;  });
		for (auto const& list : userLists) {
			result.push_back(newTreeViewItemForPatchList(list));
		}
		auto addNewItem = new TreeViewNode("Add new list", "");
		addNewItem->onSingleClick = [this](String id) {
            juce::ignoreUnused(id);
			CreateListDialog::showCreateListDialog(nullptr, TopLevelWindow::getActiveTopLevelWindow(), [this](std::shared_ptr<midikraft::PatchList> list, CreateListDialog::TFillParameters fillParameters) {
				if (list) {
					if (onPatchListFill) {
						onPatchListFill(list, fillParameters, [this, list]() {
							db_.putPatchList(list);
							regenerateUserLists([]() {});
							spdlog::info("Create new user list named {}", list->name());
							});
					}
					else {
						db_.putPatchList(list);
						regenerateUserLists([]() {});
						spdlog::info("Create new user list named {}", list->name());
					}
				}
				}, nullptr);
		};
		result.push_back(addNewItem);
		return result;
	};
	userListsItem_->onSingleClick = [this](String) {
		userListsItem_->toggleOpenness();
	};
	userListsItem_->acceptsItem = [](juce::var dropItem) {
		String dropItemString = dropItem;
		auto infos = midikraft::PatchHolder::dragInfoFromString(dropItemString.toStdString());
		return midikraft::PatchHolder::dragItemIsList(infos);
	};
	userListsItem_->onItemDropped = [this](juce::var dropItem, int) {
		String dropItemString = dropItem;
		auto infos = midikraft::PatchHolder::dragInfoFromString(dropItemString.toStdString());
		if (infos.contains("list_id") && infos.contains("list_name")) {
			// Create a new list as a copy of the dropped list (or bank)
			auto loaded_list = db_.getPatchList({ infos["list_id"], infos["list_name"] }, synths_);
			if (loaded_list) {
			auto copyOfList = std::make_shared<midikraft::PatchList>(fmt::format("Copy of {}", loaded_list->name()));
			copyOfList->setPatches(loaded_list->patches());
			db_.putPatchList(copyOfList);
			refreshAllUserLists([]() {});
		}
		}
	};

	TreeViewNode* root = new TreeViewNode("ROOT", "");
	root->onGenerateChildren = [this]() {
		return std::vector<TreeViewItem*>({ allPatchesItem_, userListsItem_ });
	};
	treeView_->setRootItem(root);
	treeView_->setRootItemVisible(false);

	// Initial openness
	allPatchesItem_->setOpenness(TreeViewItem::Openness::opennessOpen);
	userListsItem_->setOpenness(TreeViewItem::Openness::opennessOpen);

	treeView_->setColour(TreeView::selectedItemBackgroundColourId, ColourHelpers::getUIColour(this, LookAndFeel_V4::ColourScheme::highlightedFill));

	UIModel::instance()->currentSynth_.addChangeListener(this);
	UIModel::instance()->multiMode_.addChangeListener(this);
	UIModel::instance()->synthList_.addChangeListener(this);
	UIModel::instance()->importListChanged_.addChangeListener(this);
	UIModel::instance()->databaseChanged.addChangeListener(this);
}

PatchListTree::~PatchListTree()
{
	UIModel::instance()->currentSynth_.removeChangeListener(this);
	UIModel::instance()->multiMode_.removeChangeListener(this);
	UIModel::instance()->synthList_.removeChangeListener(this);
	UIModel::instance()->importListChanged_.removeChangeListener(this);
	UIModel::instance()->databaseChanged.removeChangeListener(this);
	treeView_->deleteRootItem(); // Deletes the rest as well
	CreateListDialog::release();
}

void PatchListTree::regenerateUserLists(std::function<void()> onFinished) {
	// Need to refresh user lists
	TreeViewNode* node = dynamic_cast<TreeViewNode*>(userListsItem_);
	if (node) {
		MessageManager::callAsync([this, node, onFinished]() {
			node->regenerate();
			selectAllIfNothingIsSelected();
			onFinished();
		});
	}
}

void PatchListTree::regenerateImportLists(std::function<void()> onFinished) {
	// Need to refresh user lists
	TreeViewNode* node = dynamic_cast<TreeViewNode*>(allPatchesItem_);
	if (node) {
		MessageManager::callAsync([this, node, onFinished]() {
			node->regenerate();
			selectAllIfNothingIsSelected();
			onFinished();
			});
	}
}

void PatchListTree::resized()
{
	auto area = getLocalBounds();
	treeView_->setBounds(area);
}

void PatchListTree::refreshAllUserLists(std::function<void()> onFinished)
{
	MessageManager::callAsync([this, onFinished]() {
		userListsItem_->regenerate();
		selectAllIfNothingIsSelected();
		onFinished();
	});
}

void PatchListTree::refreshAllImports(std::function<void()> onFinished)
{
	MessageManager::callAsync([this, onFinished]() {
		allPatchesItem_->regenerate();
		onFinished();
		});
}

TreeViewNode* PatchListTree::findNodeForListID(std::string const& list_id) {
	// Walk the tree and find the node for the given list id
	std::deque<juce::TreeViewItem*> items;
	items.push_back(treeView_->getRootItem());
	while (!items.empty()) {
		TreeViewItem* node = items.front();
		items.pop_front();
		// Check if this is a node for the list we're looking for
		auto treeviewnode = dynamic_cast<TreeViewNode*>(node);
		if (treeviewnode && (treeviewnode->id().toStdString() == list_id)) {
				return treeviewnode;
		}

		// Inspect the children
		for (int i = 0; i < node->getNumSubItems(); i++) {
			items.push_back(node->getSubItem(i));
		}
	}
	return nullptr;
}

void PatchListTree::refreshChildrenOfListId(std::string const& list_id, std::function<void()> onFinished) {
	MessageManager::callAsync([this, list_id, onFinished] {
		auto node = findNodeForListID(list_id);
		if (node != nullptr) {
			node->regenerate();
			onFinished();
		}
		else
		{
			spdlog::error("Program error: Did not find node for list ID {}, failed to refresh tree view", list_id);
		};
		});
}

void PatchListTree::refreshParentOfListId(std::string const& list_id, std::function<void()> onFinished) {
	MessageManager::callAsync([this, list_id, onFinished] {
		auto node = findNodeForListID(list_id);
		if (node != nullptr) {
			// Found, fresh the parent
			auto parent = node->getParentItem();
			auto parentitem = dynamic_cast<TreeViewNode*>(parent);
			if (parentitem) {
				parentitem->regenerate();
				onFinished();
				return;
			}
			else {
				spdlog::error("Program error: Parent has no regenerate capability, failed to refresh tree view");
				return;
			}
		}
		else
		{
			spdlog::error("Program error: Did not find node for list ID {}, failed to refresh tree view", list_id);
		};
		});
}

void PatchListTree::selectAllIfNothingIsSelected()
{
	if (treeView_->getNumSelectedItems() == 0) {
		allPatchesItem_->setSelected(true, false, sendNotificationAsync);
	}
}

void PatchListTree::selectItemByPath(std::vector<std::string> const& path)
{
	auto node = treeView_->getRootItem();
	TreeViewNode* child = nullptr;
	size_t index = 0;
	while (index < path.size()) {
		bool level_found = false;
		if (!node->isOpen()) {
			node->setOpen(true);
		}
		for (int c = 0; c < node->getNumSubItems(); c++) {
			child = dynamic_cast<TreeViewNode*>(node->getSubItem(c));
			if (!level_found && child && child->id().toStdString() == path[index]) {
				node = child;
				level_found = true;
				break;
			}
		}
		if (!level_found) {
			spdlog::warn("Did not find item in tree: {}", path[index]);
			return;
		}
		else {
			index++;
		}
	}
	if (node) {
		node->setSelected(true, true);
	}
	else {
		selectAllIfNothingIsSelected();
	}
}

TreeViewNode* PatchListTree::newTreeViewItemForPatch(midikraft::ListInfo list, midikraft::PatchHolder patchHolder, int index) {
	auto node = new TreeViewNode(patchHolder.name(), patchHolder.md5());
	//TODO - this doesn't work. The TreeView from JUCE has no handlers for selected or clicked that do not fire if a drag is started, so 
	// you can do either the one thing or the other.
	node->onSelected = [this, patchHolder](String md5) {
        juce::ignoreUnused(md5);
		if (onPatchSelected)
			onPatchSelected(patchHolder);
	};
	node->onItemDragged = [patchHolder, list, index]() {
		nlohmann::json dragInfo{ { "drag_type", "PATCH_IN_LIST"},
			{ "list_id", list.id},
			{ "list_name", list.name},
			{ "order_num", index },
			{ "synth", patchHolder.smartSynth()->getName()},
			{ "data_type", patchHolder.patch()->dataTypeID()},
			{ "md5", patchHolder.md5()},
			{ "patch_name", patchHolder.name() } };
		return var(dragInfo.dump(-1, ' ', true, nlohmann::detail::error_handler_t::replace));
	};
	return node;
}

TreeViewNode* PatchListTree::newTreeViewItemForSynthBanks(std::shared_ptr<midikraft::SimpleDiscoverableDevice> device) {
	std::string synthName = device->getName();
	auto synthBanksNode = new TreeViewNode("In synth", kSynthBanksPrefix + synthName);
	auto synth = std::dynamic_pointer_cast<midikraft::Synth>(device);
	if (synth) {
		synthBanksNode->onSingleClick = [synthBanksNode](String) {
			synthBanksNode->toggleOpenness();
		};
		synthBanksNode->onGenerateChildren = [this, synth, synthName] {
			std::vector<TreeViewItem*> result;

			//TODO this should be moved into a helper static function
			size_t numberOfBanks = 0;
			auto bankDescriptor = synth->getCapability<midikraft::HasBankDescriptorsCapability>();
			if (bankDescriptor)
			{
				numberOfBanks = bankDescriptor->bankDescriptors().size();
			}
			else {
				auto hasBanks = synth->getCapability<midikraft::HasBanksCapability>();
				if (hasBanks)
				{
					numberOfBanks = hasBanks->numberOfBanks();
				}
			}

			// Build a set of IDs of Banks that already are synced
			auto alreadyLoadedBanks = db_.allSynthBanks(synth);
			std::set<std::string> loadedIds;
			std::transform(alreadyLoadedBanks.begin(), alreadyLoadedBanks.end(),
				std::inserter(loadedIds, loadedIds.begin()), [](midikraft::ListInfo info) { return info.id; });

			for (int i = 0; i < (int) numberOfBanks; i++) {
				int sizeOfBank = midikraft::SynthBank::numberOfPatchesInBank(synth, i);
				auto bank_id = midikraft::ActiveSynthBank::makeId(synth, MidiBankNumber::fromZeroBase(i, sizeOfBank));
				auto bank_name = midikraft::SynthBank::friendlyBankName(synth, MidiBankNumber::fromZeroBase(i, sizeOfBank));
				auto bank = new TreeViewNode(bank_name, bank_id);
				bank->onSelected = [synth, i, this, sizeOfBank, bank](String) {
					if (onSynthBankSelected) {
						onSynthBankSelected(synth, MidiBankNumber::fromZeroBase(i, sizeOfBank));
						bank->removeTextColour();
						bank->repaintItem();
					}
				};
				bank->onItemDragged = [bank_id, bank_name]() {
					nlohmann::json dragInfo{ { "drag_type", "LIST"}, {"list_sub_type", "synth bank"}, { "list_id", bank_id}, { "list_name", bank_name } };
					return var(dragInfo.dump(-1, ' ', true, nlohmann::detail::error_handler_t::replace));
				};
				if (loadedIds.find(bank_id) == loadedIds.end()) {
					bank->setTextColour(juce::Colours::indianred);
				}
				result.push_back(bank);
			}
			return result;
		};

	}
	return synthBanksNode;
}

bool isBankCompatible(juce::var dropItem)
{
	//TODO This should already if it is the correct synth, and later if the bank is of same type
	String dropItemString = dropItem;
	auto infos = midikraft::PatchHolder::dragInfoFromString(dropItemString.toStdString());
	spdlog::debug("Dragged {}", infos.dump());
	return midikraft::PatchHolder::dragItemIsList(infos) &&
		(infos["list_sub_type"] == "synth bank" || infos["list_sub_type"] == "user bank");
}

TreeViewNode* PatchListTree::newTreeViewItemForStoredBanks(std::shared_ptr<midikraft::SimpleDiscoverableDevice> device) {
	std::string synthName = device->getName();
	auto synthBanksNode = new TreeViewNode("User Banks", kUserBanksPrefix + synthName);
	auto synth = std::dynamic_pointer_cast<midikraft::Synth>(device);
	if (synth) {
		synthBanksNode->onGenerateChildren = [this, synth, synthName, synthBanksNode] {
			std::vector<TreeViewItem*> result;
			auto userLists = db_.allUserBanks(synth);
			userLists = sortLists<midikraft::ListInfo>(userLists, [](const midikraft::ListInfo& info) { return info.name;  });
			for (auto const& list : userLists) {
				result.push_back(newTreeViewItemForUserBank(synth, synthBanksNode, list));
			}
			auto addNewItem = new TreeViewNode("Add new user bank", "");
			addNewItem->onSingleClick = [this, synth, synthBanksNode](String id) {
                juce::ignoreUnused(id);
				CreateListDialog::showCreateListDialog(nullptr, synth, TopLevelWindow::getActiveTopLevelWindow(), [this, synthBanksNode](std::shared_ptr<midikraft::PatchList> list, CreateListDialog::TFillParameters fillParameters) {
					if (list) {
						if (onPatchListFill) {
							onPatchListFill(list, fillParameters, [this, synthBanksNode, list]() {
								db_.putPatchList(list);
								spdlog::info("Created new user bank named {}", list->name());
								MessageManager::callAsync([this, synthBanksNode]() {
									synthBanksNode->regenerate();
									regenerateUserLists([]() {});
									});
								});
						}
						else {
							db_.putPatchList(list);
							spdlog::info("Created new user bank named {}", list->name());
							MessageManager::callAsync([this, synthBanksNode]() {
								synthBanksNode->regenerate();
								regenerateUserLists([]() {});
							});
						}
					}
					}, [synthBanksNode](std::shared_ptr<midikraft::PatchList> result) {
						ignoreUnused(result);
						MessageManager::callAsync([synthBanksNode]() {
							synthBanksNode->regenerate();
							});
					});
			};

			result.push_back(addNewItem);
			return result;
		};
		synthBanksNode->onSingleClick = [synthBanksNode](String) {
			synthBanksNode->toggleOpenness();
		};
		synthBanksNode->acceptsItem = [](juce::var dropItem) {
			return isBankCompatible(dropItem);
		};
		synthBanksNode->onItemDropped = [this, synthBanksNode](juce::var dropItem, int) {
			String dropItemString = dropItem;
			auto infos = midikraft::PatchHolder::dragInfoFromString(dropItemString.toStdString());
			if (isBankCompatible(dropItem)) {
				if (infos.contains("list_id") && infos.contains("list_name")) {
					// Create a new list as a copy of the dropped ban)
					auto loaded_list = db_.getPatchList({ infos["list_id"], infos["list_name"] }, synths_);
					auto loaded_bank = std::dynamic_pointer_cast<midikraft::SynthBank>(loaded_list);
					if (loaded_bank) {
						auto copyOfList = std::make_shared<midikraft::UserBank>(Uuid().toString().toStdString()
							, fmt::format("Copy of {}", loaded_list->name())
							, loaded_bank->synth()
							, loaded_bank->bankNumber());
						copyOfList->setPatches(loaded_list->patches());
						if (auto userBankSource = std::dynamic_pointer_cast<midikraft::UserBank>(loaded_list)) {
							// This is a user bank, open the dialog to let the user specify name and new bank number
							CreateListDialog::showCreateListDialog(nullptr,
								copyOfList->synth(),
								TopLevelWindow::getActiveTopLevelWindow(),
								[this, synthBanksNode, loaded_list](std::shared_ptr<midikraft::PatchList> new_list, CreateListDialog::TFillParameters ) {
									jassert(new_list);
									if (new_list) {
										// Copy over patches from droppped list to newly created list
										new_list->setPatches(loaded_list->patches());
										db_.putPatchList(new_list);
										spdlog::info("Created new user bank {} as copy of {}", new_list->name(), loaded_list->name());
										MessageManager::callAsync([synthBanksNode]() {
											synthBanksNode->regenerate();
											});
									}
								}, {});

						}
						else {
							// This is a synth bank, directly put the new user bank based on it in the database
							db_.putPatchList(copyOfList);
							regenerateImportLists([]() {}); // This refreshes the upper tree
							spdlog::info("Created new user bank {} as copy of {}", copyOfList->name(), loaded_list->name());
						}
					}
					else {
						spdlog::error("Program error - dropped list was not a synth bank after all, can't create new user list");
					}
				}

			}
		};
	}
	return synthBanksNode;
}

TreeViewNode* PatchListTree::newTreeViewItemForImports(std::shared_ptr<midikraft::SimpleDiscoverableDevice> synth) {
	std::string synthName = synth->getName();
	auto importsForSynth = new TreeViewNode("By import", kImportsTreePrefix + synthName);
	importsForSynth->onGenerateChildren = [this, synthName]() {
		auto importList = db_.getImportsList(UIModel::instance()->synthList_.synthByName(synthName).synth().get());
		shortenImportNames(importList);
		importList = sortLists<midikraft::ImportInfo>(importList, [](const midikraft::ImportInfo& import) { return import.name;  });
		std::vector<TreeViewItem*> result;
		for (auto const& import : importList) {
			auto node = new TreeViewNode(import.name, import.id, true);
			node->onSelected = [this, synthName](String id) {
				UIModel::instance()->currentSynth_.changeCurrentSynth(UIModel::instance()->synthList_.synthByName(synthName).synth());
				UIModel::instance()->multiMode_.setMultiSynthMode(false);
				if (onImportListSelected)
					onImportListSelected(id);
			};
			node->textValue.addListener(new ImportNameListener(db_, synthName, import.id));
			result.push_back(node);
		}
		return result;
	};
	importsForSynth->onSingleClick = [importsForSynth](String) {
		importsForSynth->toggleOpenness();
	};
	return importsForSynth;
}

TreeViewNode* PatchListTree::newTreeViewItemForUserBank(std::shared_ptr<midikraft::Synth> synth, TreeViewNode *parent, midikraft::ListInfo list) {
	auto node = new TreeViewNode(list.name, list.id);
	node->onSelected = [this, list, synth](String clicked) {
        juce::ignoreUnused(clicked);
		UIModel::instance()->multiMode_.setMultiSynthMode(false);
		// Need to set SYNTH
		if (onUserBankSelected)
			onUserBankSelected(synth, list.id);
	};
	node->onItemDragged = [list]() {
		nlohmann::json dragInfo{ { "drag_type", "LIST"}, {"list_sub_type", "user bank"},  {"list_id", list.id}, {"list_name", list.name}};
		return var(dragInfo.dump(-1, ' ', true, nlohmann::detail::error_handler_t::replace));
	};
	node->onDoubleClick = [node, synth, this, parent](String id) {
		// Open rename dialog on double click
		std::string oldname = node->text().toStdString();
		auto listInfo = midikraft::ListInfo({ id.toStdString(), oldname});
		auto bank = db_.getPatchList(listInfo, synths_);
		CreateListDialog::showCreateListDialog(std::dynamic_pointer_cast<midikraft::SynthBank>(bank),
			synth,
			TopLevelWindow::getActiveTopLevelWindow(),
			[this, oldname, parent](std::shared_ptr<midikraft::PatchList> new_list, CreateListDialog::TFillParameters) {
				jassert(new_list);
				if (new_list) {
					db_.putPatchList(new_list);
					spdlog::info("Renamed bank from {} to {}", oldname, new_list->name());
					MessageManager::callAsync([parent]() {
						parent->regenerate();
						});
				}
			}, [this, parent](std::shared_ptr<midikraft::PatchList> new_list) {
				if (new_list) {
					db_.deletePatchlist(midikraft::ListInfo({ new_list->id(), new_list->name() }));
					spdlog::info("Deleted user bank {}", new_list->name());
					MessageManager::callAsync([parent]() {
						parent->regenerate();
						});
				}
			});
	};
	return node;
}

TreeViewNode* PatchListTree::newTreeViewItemForPatchList(midikraft::ListInfo list) {
	auto node = new TreeViewNode(list.name, list.id);
	node->onGenerateChildren = [this, list]() {
		auto patchList = db_.getPatchList(list, synths_);
		std::vector<TreeViewItem*> result;
		if (patchList) {
			int index = 0;
			for (auto patch : patchList->patches()) {
				result.push_back(newTreeViewItemForPatch(list, patch, index++));
			}
		}
		return result;
	};
	node->onSelected = [this, list](String clicked) {
        juce::ignoreUnused(clicked);
		UIModel::instance()->multiMode_.setMultiSynthMode(true);
		if (onUserListSelected)
			onUserListSelected(list.id);
	};
	node->acceptsItem = [list](juce::var dropItem) {
		String dropItemString = dropItem;
		auto infos = midikraft::PatchHolder::dragInfoFromString(dropItemString.toStdString());
		return midikraft::PatchHolder::dragItemIsPatch(infos) || (midikraft::PatchHolder::dragItemIsList(infos) && infos["list_id"] != list.id);
	};
	node->onItemDropped = [this, list, node](juce::var dropItem, int insertIndex) {
		String dropItemString = dropItem;
		auto infos = midikraft::PatchHolder::dragInfoFromString(dropItemString.toStdString());
		if (midikraft::PatchHolder::dragItemIsPatch(infos)) {
			int position = insertIndex;
			ignoreUnused(position);
			if (!(infos.contains("synth") && infos["synth"].is_string() && infos.contains("md5") && infos["md5"].is_string())) {
				spdlog::error("Drop operation didn't give synth and md5");
				return;
			}

			std::string synthname = infos["synth"];
			if (synths_.find(synthname) == synths_.end()) {
				spdlog::error("Synth unknown during drop operation: {}", synthname);
				return;
			}

			auto synth = synths_[synthname].lock();
			std::string md5 = infos["md5"];
			std::vector<midikraft::PatchHolder> patch;
			if (db_.getSinglePatch(synth, md5, patch) && patch.size() == 1) {
				if (infos.contains("list_id") && infos["list_id"] == list.id && infos.contains("order_num")) {
					// Special case - this is a patch reference from the same list, this is effectively just a reordering operation!
					db_.movePatchInList(list, patch[0], infos["order_num"], insertIndex);
				}
				else {
					// Simple case - new patch (or patch reference) added to list
					db_.addPatchToList(list, patch[0], insertIndex);
					spdlog::info("Patch {} added to list {}", patch[0].name(), list.name);
				}
			}
			else {
				spdlog::error("Invalid drop - none or multiple patches found in database with that identifier. Program error!");
			}
		}
		else if (midikraft::PatchHolder::dragItemIsList(infos)) {
			if (infos.contains("list_id") && infos.contains("list_name")) {
				// Add all patches of the dragged list to the target ist
				auto loaded_list = db_.getPatchList({ infos["list_id"], infos["list_name"] }, synths_);
				if (AlertWindow::showOkCancelBox(AlertWindow::AlertIconType::QuestionIcon, "Add list to list?"
					, fmt::format("This will add all {} patches of the list '{}' to the list '{}' at the given position. Continue?", loaded_list->patches().size(), (std::string) infos["list_name"], list.name
					))) {
					for (auto& patch : loaded_list->patches()) {
						db_.addPatchToList(list, patch, insertIndex++);
						spdlog::info("Patch {} added to list {}", patch.name(), list.name);
					}
				}
			}
			else {
				spdlog::error("Program error - dropped list does not contain name and id!");
			}
		}
		MessageManager::callAsync([node]() {
			node->regenerate();
			node->setOpenness(TreeViewItem::Openness::opennessOpen);
			});
		if (onUserListChanged) {
			onUserListChanged(list.id);
		}
	};
	node->onItemDragged = [list]() {
		nlohmann::json dragInfo{ { "drag_type", "LIST"}, {"list_sub_type", "patch list"}, { "list_id", list.id }, { "list_name", list.name } };
		return var(dragInfo.dump(-1, ' ', true, nlohmann::detail::error_handler_t::replace));
	};
	node->onDoubleClick = [node, this](String id) {
        juce::ignoreUnused(id);
		// Open rename dialog on double click
		std::string oldname = node->text().toStdString();
		CreateListDialog::showCreateListDialog(std::make_shared<midikraft::PatchList>(node->id().toStdString(), node->text().toStdString()),
			TopLevelWindow::getActiveTopLevelWindow(),
			[this, oldname](std::shared_ptr<midikraft::PatchList> new_list, CreateListDialog::TFillParameters) {
				jassert(new_list);
				if (new_list) {
					db_.putPatchList(new_list);
					spdlog::info("Renamed list from {} to {}", oldname, new_list->name());
					regenerateUserLists([this]() { selectAllIfNothingIsSelected();  });
				}
			}, [this](std::shared_ptr<midikraft::PatchList> new_list) {
				if (new_list) {
					db_.deletePatchlist(midikraft::ListInfo({ new_list->id(), new_list->name() }));
					spdlog::info("Deleted list {}", new_list->name());
					regenerateUserLists([this]() { selectAllIfNothingIsSelected();  });
				}
			});
	};
	return node;
}


void PatchListTree::selectSynthLibrary(std::string const& synthName) {
	selectItemByPath({ kAllPatchesTree, kLibraryTreePrefix + synthName });
}

std::string PatchListTree::getSelectedSynth() const {
	auto selectedPath = pathOfSelectedItem();
	for (auto item : selectedPath) {
		if (item.rfind(kLibraryTreePrefix, 0) == 0) {
			return item.substr(kLibraryTreePrefix.size());
		}
	}
	return "";
}

bool PatchListTree::isUserListSelected() const {
	auto selectedPath = pathOfSelectedItem();
	return std::any_of(selectedPath.cbegin(), selectedPath.cend(), [](std::string const& item) { return item == kUserListsTree; });
}

std::list<std::string> PatchListTree::pathOfSelectedItem() const {
	std::list<std::string> result;
	if (treeView_->getNumSelectedItems() > 0) {
		auto item = dynamic_cast<TreeViewNode*>(treeView_->getSelectedItem(0));
		while (item != nullptr) {
			result.push_front(item->id().toStdString());
			item = dynamic_cast<TreeViewNode*>(item->getParentItem());
		}
	}
	return result;
}

void PatchListTree::changeListenerCallback(ChangeBroadcaster* source)
{
	if (source == &UIModel::instance()->currentSynth_) {
		if (UIModel::currentSynth()) {
			// Synth has changed, we may need to switch to the synth library item - if and only if a synth-specific list of another synth is selected
			if (!isUserListSelected() && getSelectedSynth() != UIModel::currentSynth()->getName()) {
				selectSynthLibrary(UIModel::currentSynth()->getName());
			}
		}
	}
	else if (source == &UIModel::instance()->importListChanged_) {
		// Did we have a previous synth/state? Then store it!
		/*if (!previousSynthName_.empty()) {
			synthSpecificTreeState_[previousSynthName_].reset(treeView_->getOpennessState(true).release());
			jassert(synthSpecificTreeState_[previousSynthName_]);
		}*/
		// Now the previous synth is the current synth
		//previousSynthName_ = UIModel::currentSynth()->getName();

		// Iterate nodes and regenerate in case group node and open!
		auto root = allPatchesItem_;
		auto currentNode = root;
		while (currentNode != nullptr) {
			for (int child = 0; child < currentNode->getNumSubItems(); child++) {
				auto subItem = currentNode->getSubItem(child);
				if (subItem->mightContainSubItems() && subItem->isOpen()) {
					subItem->clearSubItems();
					TreeViewNode* node = dynamic_cast<TreeViewNode*>(subItem);
					if (node) {
						MessageManager::callAsync([node]() {
							node->regenerate();
							});
					}
					else {
						jassertfalse;
					}
				}
			}
			currentNode = nullptr; // No recursion here, would it make sense?
		}

		// Try to restore the Tree state, if we had one stored for this synth!
		/*if (synthSpecificTreeState_.find(previousSynthName_) != synthSpecificTreeState_.end() && synthSpecificTreeState_[previousSynthName_]) {
			treeView_->restoreOpennessState(*synthSpecificTreeState_[previousSynthName_], true);
		}
		else {
			// Nothing has been stored, we have not been here before - just select the "All Patches" node
			allPatchesItem_->setSelected(true, false, sendNotificationAsync);
		}*/
	}
	else if (dynamic_cast<CurrentSynthList*>(source)) {
		// List of synths changed - we need to regenerate the imports list and the library subtrees!
		MessageManager::callAsync([this]() {
			allPatchesItem_->regenerate();
			selectAllIfNothingIsSelected();
			});
	}
	else if (source == &UIModel::instance()->databaseChanged) {
		MessageManager::callAsync([this]() {
			allPatchesItem_->regenerate();
			userListsItem_->regenerate();
			selectAllIfNothingIsSelected();
			});
	}
}
