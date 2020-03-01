/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "JuceHeader.h"

#include "PropertyEditor.h"

#include "PatchHolder.h"

class PatchNameDialog : public Component, private TextButton::Listener {
public:
	PatchNameDialog(std::function<void(midikraft::PatchHolder *result)> callback);
	virtual ~PatchNameDialog() = default;

	void setPatch(midikraft::PatchHolder *patch);

	virtual void resized() override;

	static void showPatchNameDialog(midikraft::PatchHolder *patch, Component *centeredAround, std::function<void(midikraft::PatchHolder *)> callback);
	static void release();

	void notifyResult();

private:
	static std::unique_ptr<PatchNameDialog> sPatchNameDialog_;
	static DialogWindow *sWindow_;

	void buttonClicked(Button*) override;
	std::function<void(midikraft::PatchHolder *result)> callback_;

	PropertyEditor propertyEditor_;
	TextButton ok_;
	TextButton cancel_;

	midikraft::PatchHolder  *patch_;
	std::vector<Value> names_;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PatchNameDialog)
};

