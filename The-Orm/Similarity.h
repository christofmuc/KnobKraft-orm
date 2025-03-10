#pragma once

#include "PatchDatabase.h"

class ExactSimilaritySearch;

enum class SimilarityMetric {
	L2 = 0,  // Euclidian distance squared
	IP = 1   // Inner product
};

class PatchSimilarity {
public:
	PatchSimilarity(midikraft::PatchDatabase &db);

	std::vector<midikraft::PatchHolder> findSimilarPatches(midikraft::PatchHolder const& examplePatch, int k, SimilarityMetric metric, float distance_cutoff);

private:
	midikraft::PatchDatabase& db_;
	struct pimpl_deleter { void operator()(ExactSimilaritySearch*) const; };
	std::unique_ptr<ExactSimilaritySearch, pimpl_deleter> impl_;
};
