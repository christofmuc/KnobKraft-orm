/*
   Copyright (c) 2025 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include <memory>
#include <string>

#include "SynthBank.h"

namespace knobkraft {

std::shared_ptr<midikraft::UserBank> createUserBank(std::shared_ptr<midikraft::Synth> synth,
	int bankSelected,
	std::string const& name,
	std::string const& id = {});

} // namespace knobkraft
