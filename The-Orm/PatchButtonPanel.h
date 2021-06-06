/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "JuceHeader.h"

#include "PatchHolderButton.h"
#include "PatchButtonGrid.h"

#include "MidiController.h"
#include "Synth.h"

class PatchButtonPanel : public Component,
	private Button::Listener, private ChangeListener
{
public:
	typedef std::function<void(int, int, std::function<void(std::vector<midikraft::PatchHolder>)>)> TPageLoader;

	PatchButtonPanel(std::function<void(midikraft::PatchHolder &)> handler);
	virtual ~PatchButtonPanel();

	void setPatchLoader(TPageLoader pageGetter);
	void setTotalCount(int totalCount);
	void setPatches(std::vector<midikraft::PatchHolder> const &patches, int autoSelectTarget = -1);
	
	void refresh(bool async, int autoSelectTarget = -1);

	void resized() override;

	void buttonClicked(Button* button) override;
	void buttonClicked(int buttonIndex);

	// Remote control
	void selectPrevious();
	void selectNext();
	void selectFirst();
	void pageUp(bool selectNext);
	void pageDown(bool selectLast);

	void jumpToPage(int pagenumber);

private:
	void changeListenerCallback(ChangeBroadcaster* source) override;

	String createNameOfThubnailCacheFile(midikraft::PatchHolder const &patch);
	File findPrehearFile(midikraft::PatchHolder const &patch);
	void refreshThumbnail(int i);
	int indexOfActive() const;
	void setupPageButtons();

	std::vector<midikraft::PatchHolder> patches_;
	std::unique_ptr<PatchButtonGrid<PatchHolderButton>> patchButtons_;
	std::function<void(midikraft::PatchHolder &)> handler_;
	TPageLoader pageLoader_;

	std::string activePatchMd5_;

	TextButton pageUp_, pageDown_;
	OwnedArray<TextButton> pageNumbers_;
	OwnedArray<Label> ellipsis_;
	int pageBase_;
	int pageNumber_;
	int pageSize_;
	int totalSize_;
	int numPages_;
	int maxPageButtons_;
	std::map<int, int> pageButtonMap_;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PatchButtonPanel)
};

