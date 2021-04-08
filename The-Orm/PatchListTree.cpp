/*
   Copyright (c) 2021 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "PatchListTree.h"

class GroupNode : public TreeViewItem {
public:
	typedef std::function<std::vector<TreeViewItem *>()> TChildGenerator;

	GroupNode(String text) : text_(text), hasGenerated_(false), hasChildren_(false) {
	}

	GroupNode(String text, TChildGenerator childGenerator) : text_(text), hasGenerated_(false), childGenerator_(childGenerator), hasChildren_(true) {
	}

	bool mightContainSubItems() override
	{
		return hasChildren_;
	}

	void itemOpennessChanged(bool isNowOpen) override
	{
		if (isNowOpen && !hasGenerated_) {
			auto children = childGenerator_();
			for (auto c : children) {
				addSubItem(c);
			}
			hasGenerated_ = true;
		}
	}

	Component* createItemComponent() override
	{
		return new Label("", text_);
	}

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GroupNode)

private:
	String text_;
	bool hasChildren_;
	bool hasGenerated_;
	TChildGenerator childGenerator_;

};

PatchListTree::PatchListTree()
{
	addAndMakeVisible(treeView_);

	TreeViewItem *all = new GroupNode("All patches");
	TreeViewItem *imports = new GroupNode("By import");
	TreeViewItem *root = new GroupNode("ROOT", [=]() { return std::vector<TreeViewItem *>({all, imports}); });
	treeView_.setRootItem(root);
}

PatchListTree::~PatchListTree()
{
	treeView_.deleteRootItem(); // Deletes the rest as well
}

void PatchListTree::resized()
{
	auto area = getLocalBounds();
	treeView_.setBounds(area);
}
