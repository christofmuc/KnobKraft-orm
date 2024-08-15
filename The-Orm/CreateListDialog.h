/*
   Copyright (c) 2021 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "JuceHeader.h"

#include "PatchList.h"
#include "SynthBank.h"
#include "PropertyEditor.h"


class CreateListDialog : public Component, private TextButton::Listener {
public:
	enum TListFillMode {
		None, 
		Top,
		Random
	};
	struct TFillParameters {
		TListFillMode fillMode;
		int number;
	};
	typedef std::function<void(std::shared_ptr<midikraft::PatchList> result)> TCallback;
	typedef std::function<void(std::shared_ptr<midikraft::PatchList> result, TFillParameters fillParameters)> TCallbackWithFill;

	CreateListDialog(std::shared_ptr<midikraft::Synth> synth, TCallbackWithFill& callback, TCallback& deleteCallback);
	CreateListDialog(TCallbackWithFill& callback, TCallback& deleteCallback);
	void setList(std::shared_ptr<midikraft::PatchList> list);

	virtual void resized() override;

	static void showCreateListDialog(std::shared_ptr<midikraft::SynthBank> list, std::shared_ptr<midikraft::Synth> synth, Component* centeredAround, TCallbackWithFill callback, TCallback deleteCallback);
	static void showCreateListDialog(std::shared_ptr<midikraft::PatchList> list, Component* centeredAround, TCallbackWithFill callback, TCallback deleteCallback);
	static void release();

	void notifyResult();

private:
	void buttonClicked(Button* button) override;

	static std::unique_ptr<CreateListDialog> sCreateListDialog_;
	static DialogWindow* sWindow_;

	std::shared_ptr<midikraft::Synth> synth_;
	MidiBankNumber bank_ = MidiBankNumber::invalid();
	bool isBank_;
	std::shared_ptr<midikraft::PatchList> list_;
	Value nameValue_;
	Value bankValue_;
	Value fillMode_;
	Value patchNumber_;
	PropertyEditor propertyEditor_;
	TextButton ok_;
	TextButton cancel_;
	TextButton delete_;
	TCallbackWithFill callback_;
	TCallback deleteCallback_;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CreateListDialog)
};