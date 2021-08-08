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

	allPatchesItem_ = new TreeViewNode("All patches", "");
	allPatchesItem_->onSelected = [this](String id) {
		UIModel::instance()->multiMode_.setMultiSynthMode(true);
		if (onImportListSelected)
			onImportListSelected(id);
	};
	allPatchesItem_->onGenerateChildren= [this]() {
		std::vector<TreeViewItem*> result;
		for (auto activeSynth : UIModel::instance()->synthList_.activeSynths()) {
			std::string synthName = activeSynth->getName();
			auto node = new TreeViewNode(synthName + " Library", "library-" + synthName);
			node->onSelected = [this, synthName](String id) {
				UIModel::instance()->currentSynth_.changeCurrentSynth(UIModel::instance()->synthList_.synthByName(synthName).synth());
				UIModel::instance()->multiMode_.setMultiSynthMode(false);
				if (onImportListSelected)
					onImportListSelected("");
			};
			result.push_back(node);
		}
		return result;
	};
	importListsItem_ = new TreeViewNode("By import", "imports");
	importListsItem_->onGenerateChildren = [this]() {
		std::vector<TreeViewItem*> result;
		for (auto activeSynth : UIModel::instance()->synthList_.activeSynths()) {
			std::string synthName = activeSynth->getName();
			auto importsForSynth = new TreeViewNode(synthName, synthName + "import");
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
			result.push_back(importsForSynth);
		}
		return result;
	};
	importListsItem_->onSingleClick = [this](String) {
		importListsItem_->toggleOpenness();
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
		return std::vector<TreeViewItem*>({ allPatchesItem_, importListsItem_, userListsItem_ }); 
	};
	treeView_->setRootItem(root);
	treeView_->setRootItemVisible(false);

	treeView_->setColour(TreeView::selectedItemBackgroundColourId, ColourHelpers::getUIColour(this, LookAndFeel_V4::ColourScheme::highlightedFill));

	UIModel::instance()->currentSynth_.addChangeListener(this);
	UIModel::instance()->multiMode_.addChangeListener(this);
	UIModel::instance()->synthList_.addChangeListener(this);
}

PatchListTree::~PatchListTree()
{
	UIModel::instance()->currentSynth_.removeChangeListener(this);
	UIModel::instance()->multiMode_.removeChangeListener(this);
	UIModel::instance()->synthList_.removeChangeListener(this);
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
	TreeViewNode* node = dynamic_cast<TreeViewNode*>(importListsItem_);
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
	importListsItem_->regenerate();
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
		for (int c = 0; c < node->getNumSubItems(); c++) {
			child = dynamic_cast<TreeViewNode *>(node->getSubItem(c));
			if (!level_found && child && child->id().toStdString() == path[index]) {
				node = child;
				if (!node->isOpen()) {
					node->setOpen(true);
				}
				level_found = true;
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
	if (child) {
		child->setSelected(true, true);
	}
	else {
		selectAllIfNothingIsSelected();
	}
}

TreeViewItem* PatchListTree::newTreeViewItemForPatch(midikraft::ListInfo list, midikraft::PatchHolder patchHolder) {
	auto node = new TreeViewNode(patchHolder.name(), patchHolder.md5());
	node->onSelected = [patchHolder](String md5) {
		// Clicking a patch in the list does the same thing as clicking it in the PatchView grid
		SimpleLogger::instance()->postMessage("Patch clicked: " + md5);
	};
	node->onItemDragged = [patchHolder, list]() {
		nlohmann::json dragInfo{ { "drag_type", "PATCH_IN_LIST"}, 
			{ "list_id", list.id},
			{ "list_name", list.name},
			{ "synth", patchHolder.smartSynth()->getName()}, 
			{ "data_type", patchHolder.patch()->dataTypeID()},
			{ "md5", patchHolder.md5()}, 
			{ "patch_name", patchHolder.name() } };
		return var(dragInfo.dump());
	};
	return node;
}

TreeViewItem* PatchListTree::newTreeViewItemForPatchList(midikraft::ListInfo list) {
	auto node = new TreeViewNode(list.name, list.id);
	userLists_[list.id] = node;
	node->onGenerateChildren = [this, list]() {
		auto patchList = db_.getPatchList(list, synths_);
		std::vector<TreeViewItem*> result;
		for (auto patch : patchList.patches()) {
			result.push_back(newTreeViewItemForPatch(list, patch));
		}
		return result;
	};
	node->onSelected = [this, list](String clicked) {
		UIModel::instance()->multiMode_.setMultiSynthMode(true);
		if (onUserListSelected)
			onUserListSelected(list.id);
	};
	node->onItemDropped = [this, list, node](juce::var dropItem) {
		String dropItemString = dropItem;
		auto infos = midikraft::PatchHolder::dragInfoFromString(dropItemString.toStdString());

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
			db_.addPatchToList(list, patch[0]);
			SimpleLogger::instance()->postMessage("Patch " + patch[0].name() + " added to list " + list.name);
			node->regenerate();
			if (onUserListChanged)
				onUserListChanged(list.id);
		}
		else {
			SimpleLogger::instance()->postMessage("Invalid drop - none or multiple patches found in database with that identifier. Program error!");
		}
	};
	node->onItemDragged = [list]() {
		nlohmann::json dragInfo{ { "drag_type", "LIST"}, { "list_id", list.id }, { "list_name", list.name } };
		return var(dragInfo.dump());
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

void PatchListTree::changeListenerCallback(ChangeBroadcaster* source)
{
	if (source == &UIModel::instance()->currentSynth_) {
		// Synth has changed, we need to regenerate the tree!

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
		importListsItem_->regenerate();
		selectAllIfNothingIsSelected();
	}
}
