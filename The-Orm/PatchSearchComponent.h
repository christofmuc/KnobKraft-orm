/*
   Copyright (c) 2021 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "PatchView.h"

#include "TextSearchBox.h"
#include "TouchButton.h"

class AdvancedFilterPanel;

class PatchSearchComponent : public Component, private ChangeListener
{
public:
	PatchSearchComponent(PatchView* patchView, PatchButtonPanel* patchButtons, midikraft::PatchDatabase& database);
		
	virtual ~PatchSearchComponent() override;

	virtual void resized() override;

	void loadFilter(midikraft::PatchFilter filter);
	
	midikraft::PatchFilter getFilter();

	void changeListenerCallback(ChangeBroadcaster* source) override;

	void rebuildDataTypeFilterBox();

	String advancedTextSearch() const;
	
private:
	void refreshWithFunction(TouchButtonFunction f, ToggleButton& buttonPressed);
	void unclickAllButtons();
	std::string currentSynthNameWithMulti();
	static bool isInMultiSynthMode();
	void updateCurrentFilter(); 
	midikraft::PatchFilter buildFilter() const;

	std::map<std::string, midikraft::PatchFilter> synthSpecificFilter_; // We store one filter per synth
	midikraft::PatchFilter multiModeFilter_; // The filter used when in multi synth mode

	PatchView* patchView_;
	PatchButtonPanel* patchButtons_;
	TextSearchBox textSearch_;
	CategoryButtons categoryFilters_;
	TouchToggleButton onlyFaves_;
	TouchToggleButton showHidden_;
	TouchToggleButton showRegular_;
	TouchToggleButton showUndecided_;
	TouchToggleButton onlyUntagged_;
	TouchToggleButton onlyDuplicates_;
	TouchToggleButton andCategories_;
	TextButton clearFilters_;
	ComboBox orderByType_;
	ComboBox buttonDisplayType_;

	midikraft::PatchDatabase& database_;
};



