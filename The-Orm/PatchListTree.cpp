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

class GroupNode : public TreeViewItem {
public:
	typedef std::function<std::vector<TreeViewItem*>()> TChildGenerator;
	typedef std::function<void(String)>  TClickedHandler;
	typedef std::function<void(juce::var)>  TDropHandler;

	GroupNode(String text, String id) : text_(text), id_(id) {
	}

	TChildGenerator onGenerateChildren;
	TClickedHandler onSelected;
	TClickedHandler onSingleClick;
	TClickedHandler onDoubleClick;
	TDropHandler    onItemDropped;

	bool mightContainSubItems() override
	{
		return onGenerateChildren != nullptr;
	}

	void itemOpennessChanged(bool isNowOpen) override
	{
		if (onGenerateChildren != nullptr && isNowOpen) {
			regenerate();
		}
	}

	void paintItem(Graphics& g, int width, int height) override
	{
		auto& lf = LookAndFeel::getDefaultLookAndFeel();
		g.setColour(lf.findColour(Label::textColourId));
		g.drawText(text_, 0, 0, width, height, Justification::centredLeft);
	}

	void paintOpenCloseButton(Graphics& g, const Rectangle<float>& area, Colour backgroundColour, bool isMouseOver) override {
		ignoreUnused(backgroundColour);
		TreeViewItem::paintOpenCloseButton(g, area, LookAndFeel::getDefaultLookAndFeel().findColour(TreeView::backgroundColourId), isMouseOver);
	}


	bool canBeSelected() const override
	{
		return onSelected != nullptr;
	}

	void itemSelectionChanged(bool isNowSelected) override
	{
		if (isNowSelected) {
			onSelected(id_);
		}
	}

	/*String getTooltip() override {
		return text_;
	}*/

	void regenerate() {
		if (onGenerateChildren) {
			clearSubItems();
			auto children = onGenerateChildren();
			for (auto c : children) {
				addSubItem(c);
			}
		}
	}

	virtual String getUniqueName() const override {
		if (id_.isNotEmpty()) {
			return id_;
		}
		else {
			return text_;
		}
	}

	virtual void itemClicked(const MouseEvent&) override {
		if (onSingleClick) {
			onSingleClick(id_);
		}
	}

	virtual void itemDoubleClicked(const MouseEvent&) override {
		if (onDoubleClick) {
			onDoubleClick(id_);
		}
	}

	bool isInterestedInDragSource(const DragAndDropTarget::SourceDetails&) override
	{
		return onItemDropped != nullptr;
	}


	void itemDropped(const DragAndDropTarget::SourceDetails& dragSourceDetails, int insertIndex) override
	{
		String name = dragSourceDetails.description;
		SimpleLogger::instance()->postMessage("Item dropped: " + name + " at " + String(insertIndex));
		onItemDropped(dragSourceDetails.description);
	}

	String id() const {
		return id_;
	}

	String text() const {
		return text_;
	}

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GroupNode)

private:
	String text_;
	String id_;
};

PatchListTree::PatchListTree(midikraft::PatchDatabase& db, std::vector<midikraft::SynthHolder> const& synths, TSelectionHandler importListHandler, TSelectionHandler userListHandler)
	: db_(db), importListHandler_(importListHandler), userListHandler_(userListHandler)
{
	treeView_ = std::make_unique<TreeView>();
	treeView_->setOpenCloseButtonsVisible(true);
	addAndMakeVisible(*treeView_);

	// Build data structure to load patch lists
	for (auto synth : synths) {
		synths_[synth.getName()] = synth.synth();
	}

	allPatchesItem_ = new GroupNode("All patches", "");
	allPatchesItem_->onSelected = [this](String id) {
		importListHandler_(id);
	};
	importListsItem_ = new GroupNode("By import", "");
	importListsItem_->onGenerateChildren = [this]() {
		std::vector<TreeViewItem*> result;
		for (auto activeSynth : UIModel::instance()->synthList_.activeSynths()) {
			std::string synthName = activeSynth->getName();
			auto importsForSynth = new GroupNode(synthName, synthName + "import");
			importsForSynth->onGenerateChildren = [this, synthName]() {
				auto importList = db_.getImportsList(UIModel::instance()->synthList_.synthByName(synthName).synth().get());
				std::sort(importList.begin(), importList.end(), [](const midikraft::ImportInfo& a, const midikraft::ImportInfo& b) {
					return a.description < b.description;
				});
				std::vector<TreeViewItem*> result;
				for (auto const& import : importList) {
					auto node = new GroupNode(import.description, import.id);
					node->onSelected = [this](String id) {
						importListHandler_(id);
					};
					result.push_back(node);
				}
				return result;
			};
			result.push_back(importsForSynth);
		}
		return result;
	};

	userListsItem_ = new GroupNode("User lists", "");
	userListsItem_->onGenerateChildren = [this]() {
		std::vector<TreeViewItem*> result;
		auto userLists = db_.allPatchLists();
		std::sort(userLists.begin(), userLists.end(), [](const midikraft::ListInfo& a, const midikraft::ListInfo& b) {
			return a.name < b.name;
		});
		for (auto const& list : userLists) {
			result.push_back(newTreeViewItemForPatchList(list));
		}
		auto addNewItem = new GroupNode("Add new list", "");
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
	}
	;
	GroupNode* root = new GroupNode("ROOT", "");
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
	GroupNode* node = dynamic_cast<GroupNode*>(userListsItem_);
	if (node) {
		node->regenerate();
		node->treeHasChanged();
		if (treeView_->getNumSelectedItems() == 0) {
			allPatchesItem_->setSelected(true, false, sendNotificationAsync);
		}
	}
}

void PatchListTree::regenerateImportLists() {
	// Need to refresh user lists
	GroupNode* node = dynamic_cast<GroupNode*>(importListsItem_);
	if (node) {
		node->regenerate();
		node->treeHasChanged();
		if (treeView_->getNumSelectedItems() == 0) {
			allPatchesItem_->setSelected(true, false, sendNotificationAsync);
		}
	}
}

void PatchListTree::resized()
{
	auto area = getLocalBounds();
	treeView_->setBounds(area);
}

TreeViewItem* PatchListTree::newTreeViewItemForPatch(midikraft::PatchHolder patchHolder) {
	auto node = new GroupNode(patchHolder.name(), patchHolder.md5());
	node->onSelected = [patchHolder](String md5) {
		// Clicking a patch in the list does the same thing as clicking it in the PatchView grid
		SimpleLogger::instance()->postMessage("Patch clicked: " + md5);
	};
	return node;
}

TreeViewItem* PatchListTree::newTreeViewItemForPatchList(midikraft::ListInfo list) {
	auto node = new GroupNode(list.name, list.id);
	node->onGenerateChildren = [this, list]() {
		auto patchList = db_.getPatchList(list, synths_);
		std::vector<TreeViewItem*> result;
		for (auto patch : patchList.patches()) {
			result.push_back(newTreeViewItemForPatch(patch));
		}
		return result;
	};
	node->onSelected = [this, list](String clicked) {
		userListHandler_(list.id);
	};
	node->onItemDropped = [this, list](juce::var dropItem) {
		String dropItemString = dropItem;
		auto infos = midikraft::PatchHolder::dragInfoFromString(dropItemString.toStdString());

		if (infos.size() != 3) {
			jassertfalse;
			return;
		}
		std::string synthname = infos[0];
		if (synths_.find(synthname) == synths_.end()) {
			SimpleLogger::instance()->postMessage("Error - synth unknown during drop operation: " + synthname);
			return;
		}

		// We are ignoring the data type (infos[1]) right now!
		auto synth = synths_[synthname].lock();
		std::string md5 = infos[2];
		std::vector<midikraft::PatchHolder> patch;
		if (db_.getSinglePatch(synth, md5, patch) && patch.size() == 1) {
			db_.addPatchToList(list, patch[0]);
		}
		else {
			SimpleLogger::instance()->postMessage("Invalid drop - none or multiple patches found in database with that identifier. Program error!");
		}
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
		previousSynthName_ = UIModel::currentSynth()->getName();

		// Iterate nodes and regenerate in case group node and open!
		auto root = treeView_->getRootItem();
		auto currentNode = root;
		while (currentNode != nullptr) {
			for (int child = 0; child < currentNode->getNumSubItems(); child++) {
				auto subItem = currentNode->getSubItem(child);
				if (subItem->mightContainSubItems() && subItem->isOpen()) {
					subItem->clearSubItems();
					GroupNode* node = dynamic_cast<GroupNode*>(subItem);
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
		// List of synths changed - we need to regenerate the imports list subtree!
		regenerateImportLists();
	}
}
