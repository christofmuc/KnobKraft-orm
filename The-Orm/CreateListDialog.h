/*
   Copyright (c) 2021 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "JuceHeader.h"

#include "PatchList.h"
#include "PropertyEditor.h"


class CreateListDialog : public Component, private TextButton::Listener {
public:
	typedef std::function<void(std::shared_ptr<midikraft::PatchList> result)> TCallback;

	CreateListDialog(std::shared_ptr<midikraft::PatchList> list, TCallback& callback);

	virtual void resized() override;

	static void showCreateListDialog(std::shared_ptr<midikraft::PatchList> list, Component* centeredAround, TCallback callback);
	static void release();

	void notifyResult();

private:
	void buttonClicked(Button* button) override;

	static std::unique_ptr<CreateListDialog> sCreateListDialog_;
	static DialogWindow* sWindow_;

	std::shared_ptr<midikraft::PatchList> list_;
	Value nameValue_;
	PropertyEditor propertyEditor_;
	TextButton ok_;
	TextButton cancel_;
	TCallback callback_;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CreateListDialog)
};