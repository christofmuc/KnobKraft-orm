/*
   Copyright (c) 2021 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "PatchListTree.h"

#include "UIModel.h"
#include "Logger.h"
#include "ColourHelpers.h"

class GroupNode : public TreeViewItem {
public:
	typedef std::function<std::vector<TreeViewItem *>()> TChildGenerator;
	typedef std::function<void(String)>  TClickedHandler;
	typedef std::function<void(juce::var)>  TDropHandler;

	GroupNode(String text, String id, TClickedHandler handler) : text_(text), id_(id), hasChildren_(false), handler_(handler) {
	}

	GroupNode(String text, TChildGenerator childGenerator, TClickedHandler clickedHandler, TDropHandler dropHandler) 
		: text_(text), handler_(clickedHandler), childGenerator_(childGenerator), hasChildren_(true) {
		dropHandler_ = dropHandler;
	}

	bool mightContainSubItems() override
	{
		return hasChildren_;
	}

	void itemOpennessChanged(bool isNowOpen) override
	{
		if (hasChildren_ && isNowOpen) {
			regenerate();
		}
	}

	void paintItem(Graphics& g, int width, int height) override
	{
		auto &lf = LookAndFeel::getDefaultLookAndFeel();
		g.setColour(lf.findColour(Label::textColourId));
		g.drawText(text_, 0, 0, width, height, Justification::centredLeft);
	}

	void paintOpenCloseButton(Graphics& g, const Rectangle<float>& area, Colour backgroundColour, bool isMouseOver) override {
		ignoreUnused(backgroundColour);
		TreeViewItem::paintOpenCloseButton(g, area, LookAndFeel::getDefaultLookAndFeel().findColour(TreeView::backgroundColourId), isMouseOver);
	}


	bool canBeSelected() const override
	{
		return handler_ != nullptr;
	}

	void itemSelectionChanged(bool isNowSelected) override
	{
		if (isNowSelected) {
			handler_(id_);
		}
	}

	/*String getTooltip() override {
		return text_;
	}*/

	void regenerate() {
		if (childGenerator_) {
			clearSubItems();
			auto children = childGenerator_();
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

	bool isInterestedInDragSource(const DragAndDropTarget::SourceDetails&) override
	{
		return dropHandler_ != nullptr;
	}


	void itemDropped(const DragAndDropTarget::SourceDetails& dragSourceDetails, int insertIndex) override
	{
		String name = dragSourceDetails.description;
		SimpleLogger::instance()->postMessage("Item dropped: " + name + " at " + String(insertIndex));
		dropHandler_(dragSourceDetails.description);
	}

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GroupNode)

private:
	String text_;
	String id_;
	bool hasChildren_;
	TChildGenerator childGenerator_;
	TClickedHandler handler_;
	TDropHandler dropHandler_;
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

	allPatchesItem_ = new GroupNode("All patches", "", [this](String id) {
		importListHandler_(id);
	});
	TreeViewItem* imports = new GroupNode("By import", [this]() {
		std::vector<TreeViewItem*> result;
		auto importList = db_.getImportsList(UIModel::currentSynth());
		std::sort(importList.begin(), importList.end(), [](const midikraft::ImportInfo& a, const midikraft::ImportInfo& b) {
			return a.description < b.description;
		});
		for (auto const& import : importList) {
			result.push_back(new GroupNode(import.description, import.id, [this](String id) {
				importListHandler_(id);
			}));
		}
		return result;
	}, nullptr, nullptr);
	TreeViewItem* lists = new GroupNode("User lists", [this]() {
		std::vector<TreeViewItem*> result;
		auto userLists = db_.allPatchLists();
		std::sort(userLists.begin(), userLists.end(), [](const midikraft::ListInfo& a, const midikraft::ListInfo& b) {
			return a.name < b.name;
		});
		for (auto const& list : userLists) {
			result.push_back(newTreeViewItemForPatchList(list));
		}
		result.push_back(new GroupNode("Add new list", "invalid", [this](String id) {
		}));
		return result;
	}, nullptr, nullptr);
	TreeViewItem *root = new GroupNode("ROOT", [=]() { return std::vector<TreeViewItem *>({ allPatchesItem_, imports, lists}); }, nullptr, nullptr);
	treeView_->setRootItem(root);
	treeView_->setRootItemVisible(false);

	treeView_->setColour(TreeView::selectedItemBackgroundColourId, ColourHelpers::getUIColour(this, LookAndFeel_V4::ColourScheme::highlightedFill));

	UIModel::instance()->currentSynth_.addChangeListener(this);
}

PatchListTree::~PatchListTree()
{
	UIModel::instance()->currentSynth_.removeChangeListener(this);
	treeView_->deleteRootItem(); // Deletes the rest as well
}

void PatchListTree::resized()
{
	auto area = getLocalBounds();
	treeView_->setBounds(area);
}

TreeViewItem* PatchListTree::newTreeViewItemForPatch(midikraft::PatchHolder patchHolder) {
	return new GroupNode(patchHolder.name(), patchHolder.md5(), [patchHolder](String md5) {
		// Clicking a patch in the list does the same thing as clicking it in the PatchView grid
		SimpleLogger::instance()->postMessage("Patch clicked: " + md5);
	});
}

TreeViewItem* PatchListTree::newTreeViewItemForPatchList(midikraft::ListInfo list) {
	return new GroupNode(list.name, [this, list]() {
		auto patchList = db_.getPatchList(list, synths_);
		std::vector<TreeViewItem*> result;
		for (auto patch : patchList.patches()) {
			result.push_back(newTreeViewItemForPatch(patch));
		}
		return result;
	}, [this, list](String clicked) {
		SimpleLogger::instance()->postMessage("List clicked: " + clicked + " with info " + list.id);
		userListHandler_(list.id);
	},
		[this, list](juce::var dropItem) {
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
	});
}

void PatchListTree::changeListenerCallback(ChangeBroadcaster* source)
{
	if (source == &UIModel::instance()->currentSynth_) {
		// Synth has changed, we need to regenerate the tree!
		
		// Did we have a previous synth/state? Then store it!
		if (!previousSynthName_.empty()) {
			synthSpecificTreeState_[previousSynthName_].reset(treeView_->getOpennessState(true).release());
			jassert(synthSpecificTreeState_[previousSynthName_]);
		}
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
					GroupNode *node = dynamic_cast<GroupNode*>(subItem);
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
		if (synthSpecificTreeState_.find(previousSynthName_) != synthSpecificTreeState_.end() && synthSpecificTreeState_[previousSynthName_]) {
			treeView_->restoreOpennessState(*synthSpecificTreeState_[previousSynthName_], true);
		}
		else {
			// Nothing has been stored, we have not been here before - just select the "All Patches" node
			allPatchesItem_->setSelected(true, false, sendNotificationAsync);
		}
	}
}
