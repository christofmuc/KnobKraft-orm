/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "JuceHeader.h"

class OrmLookAndFeel : public LookAndFeel_V4 {
public:
	//Font getTextButtonFont(TextButton&, int buttonHeight) override;
	void drawButtonText(Graphics&, TextButton&, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
};

