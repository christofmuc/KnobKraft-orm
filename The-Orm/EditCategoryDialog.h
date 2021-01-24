/*
   Copyright (c) 2021 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "JuceHeader.h"

#include "PatchDatabase.h"
#include "TypedNamedValue.h"

class EditCategoryDialog : public Component {
public:
	typedef std::function<void(std::vector < midikraft::PatchDatabase::CategoryDefinition> const &)> TCallback;
	EditCategoryDialog(midikraft::PatchDatabase &database);

	virtual void resized() override;

	static void showEditDialog(midikraft::PatchDatabase &db, Component *centeredAround, TCallback callback);

	void provideResult(TCallback callback);

private:
	void addCategory(midikraft::PatchDatabase::CategoryDefinition const &def);

	static std::unique_ptr<EditCategoryDialog> sEditCategoryDialog_;
	static DialogWindow *sWindow_;

	ListBox parameters_;
	TypedNamedValueSet props_;
	TextButton add_;
	TextButton ok_;
	TextButton cancel_;

};

