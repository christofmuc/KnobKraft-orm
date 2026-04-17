/*
   Copyright (c) 2021 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "JuceHeader.h"

#include "PatchDatabase.h"
#include "TypedNamedValue.h"

class EditCategoryDialog : public Component, private ValueTree::Listener {
public:
	typedef std::function<void(std::vector < midikraft::CategoryDefinition> const &)> TCallback;
	EditCategoryDialog();
	virtual ~EditCategoryDialog() override;

	virtual void resized() override;

	static void showEditDialog(midikraft::PatchDatabase &db, Component *centeredAround, TCallback callback);

	void refreshCategories(midikraft::PatchDatabase& db);
	void refreshData();
	void provideResult(TCallback callback);

	void clearData();
	static void shutdown();

	// ValueTree::Listener
	void valueTreePropertyChanged(ValueTree& treeWhosePropertyHasChanged, const Identifier& property) override;
	void valueTreeChildAdded(ValueTree& parentTree, ValueTree& childWhichHasBeenAdded) override;
	void valueTreeChildRemoved(ValueTree& parentTree, ValueTree& childWhichHasBeenRemoved, int indexFromWhichChildWasRemoved) override;

private:
	void addCategory(midikraft::CategoryDefinition const &def);

	static std::unique_ptr<EditCategoryDialog> sEditCategoryDialog_;
	static DialogWindow *sWindow_;

	int nextId_;

	std::unique_ptr<Component> tableHeader_;
	std::unique_ptr<ListBox> parameters_;
	ValueTree propsTree_;
	TextButton add_;
	TextButton ok_;
	TextButton cancel_;

};

