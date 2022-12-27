/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/


#include "OrmLookAndFeel.h"

#pragma warning(disable:4100) // Suppress unreferenced formal parameter warning
#include <docks/docks.h>

OrmLookAndFeel::OrmLookAndFeel() {	
	setColourScheme(LookAndFeel_V4::getMidnightColourScheme());
	auto backgroundColour = getCurrentColourScheme().getUIColour(ColourScheme::UIColour::windowBackground);
	setColour(DockingWindow::ColourIds::backgroundColourId, backgroundColour);
}

/*juce::Font OrmLookAndFeel::getTextButtonFont(TextButton& button, int buttonHeight)
{
	ignoreUnused(button, buttonHeight);
	return { 14.0f };
}*/

/*void OrmLookAndFeel::drawButtonText(Graphics &g, TextButton &button, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
	ignoreUnused(shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);

	Font font(getTextButtonFont(button, button.getHeight()));
	g.setFont(font);
	g.setColour(button.findColour(button.getToggleState() ? TextButton::textColourOnId
		: TextButton::textColourOffId)
		.withMultipliedAlpha(button.isEnabled() ? 1.0f : 0.5f));

	const int yIndent = jmin(4, button.proportionOfHeight(0.3f));
	const int cornerSize = jmin(button.getHeight(), button.getWidth()) / 2;

	const int fontHeight = roundToInt(font.getHeight() * 0.6f);
	const int leftIndent = jmin(fontHeight, 2 + cornerSize / (button.isConnectedOnLeft() ? 4 : 2));
	const int rightIndent = jmin(fontHeight, 2 + cornerSize / (button.isConnectedOnRight() ? 4 : 2));
	const int textWidth = button.getWidth() - leftIndent - rightIndent;

	if (textWidth > 0) {
		g.drawText(button.getButtonText(),
			leftIndent, yIndent, textWidth, button.getHeight() - yIndent * 2,
			Justification::centred, true);
	}
}*/
