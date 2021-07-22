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

	GroupNode(String text, String id, TClickedHandler handler) : text_(text), id_(id), hasGenerated_(false), hasChildren_(false), handler_(handler) {
	}

	GroupNode(String text, TChildGenerator childGenerator) : text_(text), hasGenerated_(false), childGenerator_(childGenerator), hasChildren_(true) {
	}

	bool mightContainSubItems() override
	{
		return hasChildren_;
	}

	void itemOpennessChanged(bool isNowOpen) override
	{
		if (hasChildren_ && isNowOpen && !hasGenerated_) {
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
		return handler_.operator bool();
	}

	void itemSelectionChanged(bool isNowSelected) override
	{
		ignoreUnused(isNowSelected);
		handler_(id_);
	}

	/*String getTooltip() override {
		return text_;
	}*/

	void regenerate() {
		if (childGenerator_) {
			auto children = childGenerator_();
			for (auto c : children) {
				addSubItem(c);
			}
			hasGenerated_ = true;
		}
	}

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GroupNode)

private:
	String text_;
	String id_;
	bool hasChildren_;
	bool hasGenerated_;
	TChildGenerator childGenerator_;
	TClickedHandler handler_;

};

PatchListTree::PatchListTree(midikraft::PatchDatabase& db, std::function<void(String)> clickHandler) : db_(db), clickHandler_(clickHandler)
{
	treeView_ = std::make_unique<TreeView>();
	treeView_->setOpenCloseButtonsVisible(true);
	addAndMakeVisible(*treeView_);

	TreeViewItem* all = new GroupNode("All patches", "***", [this](String id) {
		clickHandler_(id);
	});
	TreeViewItem* imports = new GroupNode("By import", [this]() {
		std::vector<TreeViewItem*> result;
		auto importList = db_.getImportsList(UIModel::currentSynth());
		std::sort(importList.begin(), importList.end(), [](const midikraft::ImportInfo& a, const midikraft::ImportInfo& b) {
			return a.description < b.description;
		});
		for (auto const& import : importList) {
			result.push_back(new GroupNode(import.description, import.id, [this](String id) {
				clickHandler_(id);
			}));
		}
		return result;
	});
	TreeViewItem* lists = new GroupNode("User lists", [this]() {
		std::vector<TreeViewItem*> result;
		auto userLists = db_.allPatchLists();
		std::sort(userLists.begin(), userLists.end(), [](const midikraft::ListInfo& a, const midikraft::ListInfo& b) {
			return a.name < b.name;
		});
		for (auto const& list : userLists) {
			result.push_back(new GroupNode(list.name, list.id, [this](String id) {				
			}));
		}
		result.push_back(new GroupNode("Add new list", "invalid", [this](String id) {
		}));
		return result;
	});
	TreeViewItem *root = new GroupNode("ROOT", [=]() { return std::vector<TreeViewItem *>({all, imports, lists}); });
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

void PatchListTree::changeListenerCallback(ChangeBroadcaster* source)
{
	if (source == &UIModel::instance()->currentSynth_) {
		// Synth has changed, we need to regenerate the tree!
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
					}
					else {
						jassertfalse;
					}
				}
			}
			currentNode = nullptr; // No recursion here, would it make sense?
		}
	}
}
