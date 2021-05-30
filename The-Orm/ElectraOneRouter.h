/*
   Copyright (c) 2021 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "MidiController.h"

class ElectraOneRouter {
public:
	ElectraOneRouter();
	virtual ~ElectraOneRouter();

	void enable(bool enabled);

private:
	bool enabled_;
	midikraft::MidiController::HandlerHandle routerCallback_ = midikraft::MidiController::makeNoneHandle();
};

