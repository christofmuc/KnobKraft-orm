#pragma once

#include "PatchDatabase.h"

class ExactSimilaritySearch;

class PatchSimilarity {
public:
	PatchSimilarity(midikraft::PatchDatabase &db);

	std::vector<midikraft::PatchHolder> findSimilarPatches(midikraft::PatchHolder const& examplePatch, int k);

private:
	midikraft::PatchDatabase& db_;
	std::unique_ptr<ExactSimilaritySearch> impl_;
};
