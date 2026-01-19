/*
   Copyright (c) 2026 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "doctest/doctest.h"

#include "PatchListFill.h"
#include "test_helpers.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace {

using test_helpers::DummySynth;
using test_helpers::makePatchHolder;

std::vector<midikraft::PatchHolder> makeSequentialPatches(std::shared_ptr<DummySynth> synth, int count) {
	std::vector<midikraft::PatchHolder> patches;
	patches.reserve(static_cast<size_t>(count));
	for (int i = 1; i <= count; ++i) {
		auto name = std::string("Patch ") + std::to_string(i);
		patches.push_back(makePatchHolder(synth, name, { static_cast<uint8>(i) }));
	}
	return patches;
}

} // namespace

TEST_CASE("fill list from active patch stops at end and pads with last patch") {
	auto synth = std::make_shared<DummySynth>("DummySynth", 20);
	auto patches = makeSequentialPatches(synth, 20);
	auto activePatch = patches[9];

	midikraft::PatchListFillRequest request{ midikraft::PatchListFillMode::FromActive, 0, 15 };
	auto result = midikraft::fillPatchList(patches, &activePatch, request);

	CHECK(result.activePatchFound);
	REQUIRE(result.patches.size() == 15);
	CHECK(result.patches.front().name() == "Patch 10");
	CHECK(result.patches[10].name() == "Patch 20");
	CHECK(result.patches[11].name() == "Patch 20");
	CHECK(result.patches.back().name() == "Patch 20");

	bool hasPatch1 = std::any_of(result.patches.begin(), result.patches.end(),
		[](midikraft::PatchHolder const& patch) {
			return patch.name() == "Patch 1";
		});
	CHECK_FALSE(hasPatch1);
}

TEST_CASE("fill list from active patch honors desired count") {
	auto synth = std::make_shared<DummySynth>("DummySynth", 20);
	auto patches = makeSequentialPatches(synth, 20);
	auto activePatch = patches[9];

	midikraft::PatchListFillRequest request{ midikraft::PatchListFillMode::FromActive, 8, 0 };
	auto result = midikraft::fillPatchList(patches, &activePatch, request);

	REQUIRE(result.patches.size() == 8);
	CHECK(result.patches.front().name() == "Patch 10");
	CHECK(result.patches.back().name() == "Patch 17");
}
