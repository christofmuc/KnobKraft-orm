/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "PatchHolder.h"

class ScriptedQuery {
public:
	std::vector<midikraft::PatchHolder> filterByPredicate(std::string const &pythonPredicate, std::vector<midikraft::PatchHolder> const &input) const;
};

