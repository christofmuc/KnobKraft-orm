/*
   Copyright (c) 2021 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "PatchListTree.h"

#include "CreateListDialog.h"

#include "UIModel.h"
#include "Logger.h"
#include "ColourHelpers.h"

#include <boost/format.hpp>

void shortenImportNames(std::vector<midikraft::ImportInfo>& imports) {
	for (auto& import : imports) {
		if (import.description.rfind("Imported from file") == 0) {
			import.description = import.description.substr(19);
		} else if (import.description.rfind("Imported from synth") == 0) {
			import.description = import.description.substr(20);
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

	allPatchesItem_ = new TreeViewNode("All patches", "allpatches");
	allPatchesItem_->onSelected = [this](String id) {
		UIModel::instance()->multiMode_.setMultiSynthMode(true);
		if (onImportListSelected)
			onImportListSelected("");
	};
	allPatchesItem_->onGenerateChildren = [this]() {
		std::vector<TreeViewItem*> result;
		for (auto activeSynth : UIModel::instance()->synthList_.activeSynths()) {
			std::string synthName = activeSynth->getName();
			auto synthLibrary = new TreeViewNode(synthName, "library-" + synthName);
			synthLibrary->onGenerateChildren = [activeSynth, this]() {
				return std::vector<TreeViewItem*>({ newTreeViewItemForImports(activeSynth) });
			};
			synthLibrary->onSelected = [this, synthName](String id) {
				UIModel::instance()->currentSynth_.changeCurrentSynth(UIModel::instance()->synthList_.synthByName(synthName).synth());
				UIModel::instance()->multiMode_.setMultiSynthMode(false);
				if (onImportListSelected)
					onImportListSelected("");
			};

			result.push_back(synthLibrary);
		}
		return result;
	};

	userListsItem_ = new TreeViewNode("User lists", "userlists");
	userListsItem_->onGenerateChildren = [this]() {
		std::vector<TreeViewItem*> result;
		auto userLists = db_.allPatchLists();
		userLists = sortLists<midikraft::ListInfo>(userLists, [](const midikraft::ListInfo& info) { return info.name;  });
		userLists_.clear();
		for (auto const& list : userLists) {
			result.push_back(newTreeViewItemForPatchList(list));
		}
		auto addNewItem = new TreeViewNode("Add new list", "");
		addNewItem->onSingleClick = [this](String id) {
			CreateListDialog::showCreateListDialog(nullptr, TopLevelWindow::getActiveTopLevelWindow(), [this](std::shared_ptr<midikraft::PatchList> list) {
				if (list) {
					db_.putPatchList(*list);
					SimpleLogger::instance()->postMessage("Create new user list named " + list->name());
					regenerateUserLists();
				}
			}, nullptr);
		};
		result.push_back(addNewItem);
		return result;
	};
	userListsItem_->onSingleClick = [this](String) {
		userListsItem_->toggleOpenness();
	};
	
	TreeViewNode* root = new TreeViewNode("ROOT", "");
	root->onGenerateChildren = [=]() { 
		return std::vector<TreeViewItem*>({ allPatchesItem_, userListsItem_ }); 
	};
	treeView_->setRootItem(root);
	treeView_->setRootItemVisible(false);

	// Initial openness
	allPatchesItem_->setOpenness(TreeViewItem::opennessOpen);
	userListsItem_->setOpenness(TreeViewItem::opennessOpen);

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

void PatchListTree::regenerateUserLists() {
	// Need to refresh user lists
	TreeViewNode* node = dynamic_cast<TreeViewNode*>(userListsItem_);
	if (node) {
		node->regenerate();
		selectAllIfNothingIsSelected();
	}
}

void PatchListTree::regenerateImportLists() {
	// Need to refresh user lists
	TreeViewNode* node = dynamic_cast<TreeViewNode*>(allPatchesItem_);
	if (node) {
		node->regenerate();
		selectAllIfNothingIsSelected();
	}
}

void PatchListTree::resized()
{
	auto area = getLocalBounds();
	treeView_->setBounds(area);
}

void PatchListTree::refreshAllUserLists()
{
	userListsItem_->regenerate();
	selectAllIfNothingIsSelected();
}

void PatchListTree::refreshUserList(std::string list_id)
{
	if (userLists_.find(list_id) != userLists_.end()) {
		userLists_[list_id]->regenerate();
	}
	else {
		jassertfalse;
	}
}

void PatchListTree::refreshAllImports()
{
	allPatchesItem_->regenerate();
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
	int index = 0;
	while (index < path.size()) {
		bool level_found = false;
		if (!node->isOpen()) {
			node->setOpen(true);
		}
		for (int c = 0; c < node->getNumSubItems(); c++) {
			child = dynamic_cast<TreeViewNode *>(node->getSubItem(c));
			if (!level_found && child && child->id().toStdString() == path[index]) {
				node = child;
				level_found = true;
				break;
			}
		}
		if (!level_found) {
			SimpleLogger::instance()->postMessage("Did not find item in tree");
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

TreeViewItem* PatchListTree::newTreeViewItemForPatch(midikraft::ListInfo list, midikraft::PatchHolder patchHolder, int index) {
	auto node = new TreeViewNode(patchHolder.name(), patchHolder.md5());
	//TODO - this doesn't work. The TreeView from JUCE has no handlers for selected or clicked that do not fire if a drag is started, so 
	// you can do either the one thing or the other.
	node->onSelected = [this, patchHolder](String md5) {
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

TreeViewItem* PatchListTree::newTreeViewItemForImports(std::shared_ptr<midikraft::SimpleDiscoverableDevice> synth) {
	std::string synthName = synth->getName();
	auto importsForSynth = new TreeViewNode("By import", "imports-" + synthName);
	importsForSynth->onGenerateChildren = [this, synthName]() {
		auto importList = db_.getImportsList(UIModel::instance()->synthList_.synthByName(synthName).synth().get());
		shortenImportNames(importList);
		importList = sortLists<midikraft::ImportInfo>(importList, [](const midikraft::ImportInfo& import) { return import.description;  });
		std::vector<TreeViewItem*> result;
		for (auto const& import : importList) {
			auto node = new TreeViewNode(import.description, import.id);
			node->onSelected = [this, synthName](String id) {
				UIModel::instance()->currentSynth_.changeCurrentSynth(UIModel::instance()->synthList_.synthByName(synthName).synth());
				UIModel::instance()->multiMode_.setMultiSynthMode(false);
				if (onImportListSelected)
					onImportListSelected(id);
			};
			result.push_back(node);
		}
		return result;
	};
	importsForSynth->onSingleClick = [importsForSynth](String) {
		importsForSynth->toggleOpenness();
	};
	return importsForSynth;
}

TreeViewItem* PatchListTree::newTreeViewItemForPatchList(midikraft::ListInfo list) {
	auto node = new TreeViewNode(list.name, list.id);
	userLists_[list.id] = node;
	node->onGenerateChildren = [this, list]() {
		auto patchList = db_.getPatchList(list, synths_);
		std::vector<TreeViewItem*> result;
		int index = 0;
		for (auto patch : patchList.patches()) {
			result.push_back(newTreeViewItemForPatch(list, patch, index++));
		}
		return result;
	};
	node->onSelected = [this, list](String clicked) {
		UIModel::instance()->multiMode_.setMultiSynthMode(true);
		if (onUserListSelected)
			onUserListSelected(list.id);
	};
	node->acceptsItem = [this, list](juce::var dropItem) {
		String dropItemString = dropItem;
		auto infos = midikraft::PatchHolder::dragInfoFromString(dropItemString.toStdString());
		return infos.contains("drag_type") && (infos["drag_type"] == "PATCH" || infos["drag_type"] == "PATCH_IN_LIST");
	};
	node->onItemDropped = [this, list, node](juce::var dropItem, int insertIndex) {
		String dropItemString = dropItem;
		auto infos = midikraft::PatchHolder::dragInfoFromString(dropItemString.toStdString());
		int position = insertIndex;
		ignoreUnused(position);
		if (!(infos.contains("synth") && infos["synth"].is_string() && infos.contains("md5") && infos["md5"].is_string())) {
			SimpleLogger::instance()->postMessage("Error - drop operation didn't give synth and md5");
			return;
		}

		std::string synthname = infos["synth"];
		if (synths_.find(synthname) == synths_.end()) {
			SimpleLogger::instance()->postMessage("Error - synth unknown during drop operation: " + synthname);
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
				SimpleLogger::instance()->postMessage("Patch " + patch[0].name() + " added to list " + list.name);
			}
			node->regenerate();
			node->setOpenness(TreeViewItem::Openness::opennessOpen);
			if (onUserListChanged)
				onUserListChanged(list.id);
		}
		else {
			SimpleLogger::instance()->postMessage("Invalid drop - none or multiple patches found in database with that identifier. Program error!");
		}
	};
	node->onItemDragged = [list]() {
		nlohmann::json dragInfo{ { "drag_type", "LIST"}, { "list_id", list.id }, { "list_name", list.name } };
		return var(dragInfo.dump(-1, ' ', true, nlohmann::detail::error_handler_t::replace));
	};
	node->onDoubleClick = [node, this](String id) {
		// Open rename dialog on double click
		std::string oldname = node->text().toStdString();
		CreateListDialog::showCreateListDialog(std::make_shared<midikraft::PatchList>(node->id().toStdString(), node->text().toStdString()),
			TopLevelWindow::getActiveTopLevelWindow(),
			[this, oldname](std::shared_ptr<midikraft::PatchList> list) {
			jassert(list);
			if (list) {
				db_.putPatchList(*list);
				SimpleLogger::instance()->postMessage((boost::format("Renamed list from %s to %s") % oldname % list->name()).str());
				regenerateUserLists();
			}
		}, [this](std::shared_ptr<midikraft::PatchList> list) {
			if (list) {
				db_.deletePatchlist(midikraft::ListInfo({ list->id(), list->name() }));
				SimpleLogger::instance()->postMessage("Deleted list " + list->name());
				regenerateUserLists();
			}
		});
	};
	return node;
}

void PatchListTree::selectSynthLibrary(std::string const& synthName) {
	selectItemByPath({ "allpatches", "library-" + synthName });
}

std::string PatchListTree::getSelectedSynth() const {
	auto selectedPath = pathOfSelectedItem();
	for (auto item : selectedPath) {
		if (item.rfind("library-", 0) == 0) {
			return item.substr(strlen("library-"));
		}
	}
	return "";
}

bool PatchListTree::isUserListSelected() const {
	auto selectedPath = pathOfSelectedItem();
	return std::any_of(selectedPath.cbegin(), selectedPath.cend(), [](std::string const& item) { return item == "userlists"; });
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
		// Synth has changed, we may need to switch to the synth library item - if and only if a synth-specific list of another synth is selected
		if (!isUserListSelected() && getSelectedSynth() != UIModel::currentSynth()->getName()) {
			selectSynthLibrary(UIModel::currentSynth()->getName());
		}
	}
	else if (source == &UIModel::instance()->importListChanged_) {
		// Did we have a previous synth/state? Then store it!
		/*if (!previousSynthName_.empty()) {
			synthSpecificTreeState_[previousSynthName_].reset(treeView_->getOpennessState(true).release());
			jassert(synthSpecificTreeState_[previousSynthName_]);
		}*/
		// Now the previous synth is the current synth
		/*previousSynthName_ = UIModel::currentSynth()->getName();

		// Iterate nodes and regenerate in case group node and open!
		auto root = treeView_->getRootItem();
		auto currentNode = root;
		while (currentNode != nullptr) {
			for (int child = 0; child < currentNode->getNumSubItems(); child++) {
				auto subItem = currentNode->getSubItem(child);
				if (subItem->mightContainSubItems() && subItem->isOpen()) {
					subItem->clearSubItems();
					TreeViewNode* node = dynamic_cast<TreeViewNode*>(subItem);
					if (node) {
						node->regenerate();
						node->treeHasChanged();
					}
					else {
						jassertfalse;
					}
				}
			}
			currentNode = nullptr; // No recursion here, would it make sense?
		}*/

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
		allPatchesItem_->regenerate();
		selectAllIfNothingIsSelected();
	}
	else if (source == &UIModel::instance()->databaseChanged) {
		allPatchesItem_->regenerate();
		userListsItem_->regenerate();
		selectAllIfNothingIsSelected();
	}
}
