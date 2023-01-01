/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "MidiProgramNumber.h"

#include "BCR2000.h"
#include "BCR2000Proxy.h"

namespace midikraft {

	class SupportedByBCR2000 {
	public:
		virtual void setupBCR2000(BCR2000 &bcr) = 0;
		virtual void setupBCR2000View(BCR2000Proxy *view, TypedNamedValueSet &parameterModel, ValueTree &valueTree) = 0;
	};

}
