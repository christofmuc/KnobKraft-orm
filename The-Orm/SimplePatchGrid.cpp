/*
   Copyright (c) 2023 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "SimplePatchGrid.h"

#include "PatchView.h"
#include "Data.h"
#include "UIModel.h"

#include <spdlog/spdlog.h>

SimplePatchGrid::SimplePatchGrid(PatchView* patchView) : patchView_(patchView)
{
	patchView_->registerSecondaryGrid(this);

	grid_ = std::make_unique<PatchButtonPanel>([this](midikraft::PatchHolder& patch) { patchSelectedHandler(patch); }, "secondWindow");
	addAndMakeVisible(*grid_);

	// Setup the Grid so it always shows the same list as our main patch view
	grid_->setPatchLoader([this](int skip, int limit, std::function<void(std::vector< midikraft::PatchHolder>)> callback) {
		patchView_->loadPage(skip, limit, patchView_->currentFilter(), callback);
		});

	Data::ensureEphemeralPropertyExists(EPROPERTY_LIBRARY_PATCH_LIST, {});
	listeners_.addListener(Data::getEphemeralPropertyAsValue(EPROPERTY_LIBRARY_PATCH_LIST), [this](juce::Value& newValue) {
		ignoreUnused(newValue);
		reload();
		});
	listeners_.triggerAll();
}

SimplePatchGrid::~SimplePatchGrid()
{
	if (patchView_) {
		patchView_->unregisterSecondaryGrid(this);
	}
}

void SimplePatchGrid::resized()
{
	auto bounds = getLocalBounds();
	grid_->setBounds(bounds);
}

void SimplePatchGrid::reload()
{
	grid_->setTotalCount(patchView_->getTotalCount());
	grid_->refresh(true);
}

void SimplePatchGrid::applyPatchUpdate(midikraft::PatchHolder const& patch)
{
	grid_->updateVisiblePatch(patch);
}

void SimplePatchGrid::patchSelectedHandler(midikraft::PatchHolder& patch)
{
	spdlog::info("Patch {} selected", patch.name());
	patchView_->selectPatch(patch, true);
	if (onPatchSelected)
	{
		onPatchSelected(patch);
	}
}
