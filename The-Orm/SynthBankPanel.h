/*
   Copyright (c) 2022 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "JuceHeader.h"

#include "VerticalPatchButtonList.h"
#include "SynthBank.h"
#include "PatchDatabase.h"
#include "InfoText.h"

class PatchView;

class SynthBankPanel : public Component, private ChangeListener
{
public:
	SynthBankPanel(midikraft::PatchDatabase& patchDatabase, PatchView *patchView);
	virtual ~SynthBankPanel() override;

	virtual void resized() override;

	void setBank(std::shared_ptr<midikraft::SynthBank> synthBank, PatchButtonInfo info);
	void refreshPatch(std::shared_ptr<midikraft::PatchHolder> updatedPatch);
	void reloadFromDatabase();

	std::shared_ptr<midikraft::SynthBank> getCurrentSynthBank() const;

    void copyPatchNamesToClipboard();

private:
	virtual void changeListenerCallback(ChangeBroadcaster* source) override;

	void saveToDatabase();
	void refresh();
	bool isUserBank();
	void showInfoIfRequired();

	midikraft::PatchDatabase& patchDatabase_;
	PatchView* patchView_;
	std::shared_ptr<midikraft::SynthBank> synthBank_;
	PatchButtonInfo buttonMode_;
	InfoText instructions_;
	Label synthName_;
	Label bankNameAndDate_;
	Label modified_;
	TextButton resyncButton_;
	TextButton sendButton_;
	TextButton exportButton_;
	std::unique_ptr<VerticalPatchButtonList> bankList_;
};
