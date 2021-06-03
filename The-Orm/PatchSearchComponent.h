/*
   Copyright (c) 2021 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "PatchView.h"

class AdvancedFilterPanel;

class PatchSearchComponent : public Component, private ChangeListener
{
public:
	PatchSearchComponent::PatchSearchComponent(PatchView* patchView, PatchButtonPanel* patchButtons, midikraft::PatchDatabase& database);
		
	virtual ~PatchSearchComponent();

	virtual void resized() override;

	midikraft::PatchDatabase::PatchFilter buildFilter();

	void changeListenerCallback(ChangeBroadcaster* source) override;

	void rebuildSynthFilters();
	void rebuildImportFilterBox();
	void rebuildDataTypeFilterBox();

	void selectImportByID(String id);
	void selectImportByDescription(std::string const& description);
	std::string currentlySelectedSourceUUID();
	bool atLeastOneSynth();
	String advancedTextSearch() const;
	
private:
	PatchView* patchView_;
	PatchButtonPanel* patchButtons_;
	ComboBox importList_;
	TextEditor nameSearchText_;
	ToggleButton useNameSearch_;
	CategoryButtons categoryFilters_;
	std::unique_ptr<CollapsibleContainer> advancedSearch_;
	std::unique_ptr<AdvancedFilterPanel> advancedFilters_;
	ToggleButton onlyFaves_;
	ToggleButton showHidden_;
	ToggleButton onlyUntagged_;

	midikraft::PatchDatabase& database_;
};



