/*
   Copyright (c) 2021 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "JuceHeader.h"

#include "PropertyEditor.h"
#include "PatchHolder.h"

class BulkRenameDialog : public Component, private TextButton::Listener {
public:
	typedef std::function<void(std::vector<midikraft::PatchHolder> result)> TCallback;

	BulkRenameDialog();
	void setList(std::vector<midikraft::PatchHolder> input);

	virtual void resized() override;

	static void show(std::vector<midikraft::PatchHolder> input, Component* centeredAround, TCallback callback);
	static void release();

	void notifyResult();

private:
	void buttonClicked(Button* button) override;

	static std::unique_ptr<BulkRenameDialog> sBulkRenameDialog_;
	static DialogWindow* sWindow_;

	std::vector<midikraft::PatchHolder> input_;
	PropertyEditor propertyEditor_;
	PropertyEditor::TProperties props_;
	TextButton ok_;
	TextButton cancel_;
	TextButton paste_;
	TextButton copy_;
	TextButton fromFilename_;
	TCallback callback_;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BulkRenameDialog)
};