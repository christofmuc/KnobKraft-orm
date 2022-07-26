/*
   Copyright (c) 2022 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "JuceHeader.h"

#include "VerticalPatchButtonList.h"
#include "SynthBank.h"
#include "PatchDatabase.h"

class SynthBankPanel : public Component, private ChangeListener
{
public:
	SynthBankPanel(midikraft::PatchDatabase& patchDatabase);
	virtual ~SynthBankPanel() override;

	virtual void resized() override;

	void setBank(std::shared_ptr<midikraft::SynthBank> synthBank, PatchButtonInfo info);

private:
	virtual void changeListenerCallback(ChangeBroadcaster* source) override;

	midikraft::PatchDatabase& patchDatabase_;
	std::shared_ptr<midikraft::SynthBank> synthBank_;
	Label synthName_;
	Label bankNameAndDate_;
	Label modified_;
	TextButton resyncButton_;
	std::unique_ptr<VerticalPatchButtonList> bankList_;
};
