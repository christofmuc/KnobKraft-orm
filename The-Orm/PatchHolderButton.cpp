/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "PatchHolderButton.h"

#include "ColourHelpers.h"
#include "LayeredPatchCapability.h"


Colour PatchHolderButton::buttonColourForPatch(midikraft::PatchHolder &patch, Component *componentForDefaultBackground) {
	Colour color = ColourHelpers::getUIColour(componentForDefaultBackground, LookAndFeel_V4::ColourScheme::widgetBackground);
	auto cats = patch.categories();
	if (!cats.empty()) {
		// Random in case the patch has multiple categories
		color = cats.cbegin()->color();
	}
	return color;
}

void PatchHolderButton::setPatchHolder(midikraft::PatchHolder *holder, bool active, bool showSynthName)
{
	setActive(active);
	Colour color = ColourHelpers::getUIColour(this, LookAndFeel_V4::ColourScheme::widgetBackground);
	if (holder) {
		auto layers = std::dynamic_pointer_cast<midikraft::LayeredPatchCapability>(holder->patch());
		if (layers) {
			if (layers->layerName(0) != layers->layerName(1)) {
				setButtonData(layers->layerName(0), layers->layerName(1), holder->createDragInfoString());
			}
			else {
				setButtonData(layers->layerName(0), holder->createDragInfoString());
			}
		}
		else {
			setButtonData(holder->name(), holder->createDragInfoString());
		}

		std::string subtitle = "";
		// Later, we can add a UI switch for this?
		if (true) {
			subtitle = holder->synth()->friendlyProgramName(holder->patchNumber());
		}

		setSubtitle((showSynthName && holder->synth()) ? holder->synth()->getName() : subtitle);
		setColour(TextButton::ColourIds::buttonColourId, buttonColourForPatch(*holder, this));
		setFavorite(holder->isFavorite());
		setHidden(holder->isHidden());
	}
	else {
		setButtonData("", "");
		setSubtitle("");
		setColour(TextButton::ColourIds::buttonColourId, color);
		setFavorite(false);
		setHidden(false);
	}
}
