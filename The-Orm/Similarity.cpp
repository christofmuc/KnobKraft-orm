#include "Similarity.h"

#include "Capability.h"
#include "DetailedParametersCapability.h"
#include "ProgressHandlerWindow.h"

#include "faiss/IndexFlat.h"

#include <spdlog/spdlog.h>

extern "C" {
#include <cblas.h> // For BLAS functions
}

#include <vector>
#include <cmath>
#include <cblas.h> // For BLAS functions

void normalizeVectors(float* data, size_t numVectors, size_t dimensionality) {
	// Step 1: Compute the norm of each vector
	std::vector<float> norms(numVectors);
	for (size_t i = 0; i < numVectors; i++) {
		// Compute the L2 norm of the i-th vector
		norms[i] = cblas_snrm2(dimensionality, &data[i * dimensionality], 1);
	}

	// Step 2: Normalize each vector
	for (size_t i = 0; i < numVectors; i++) {
		if (norms[i] > 0) {
			// Scale the i-th vector by 1 / norm
			cblas_sscal(dimensionality, 1.0f / norms[i], &data[i * dimensionality], 1);
		}
	}
}

float computeTrueMaxDistance(const float* data, size_t numVectors, size_t dimensionality) {
	float max_distance = 0.0f;
	std::vector<float> diff(dimensionality, 0.0f);

	for (size_t i = 0; i < numVectors; i++) {
		for (size_t j = i + 1; j < numVectors; j++) {
			// Compute difference between vector i and j
			for (size_t d = 0; d < dimensionality; d++) {
				diff[d] = data[i * dimensionality + d] - data[j * dimensionality + d];
			}
			// Compute L2 norm using BLAS
			float dist = cblas_snrm2(dimensionality, diff.data(), 1);
			// This would be squared norm:
			//float dist = cblas_sdot(dimensionality, diff.data(), 1, diff.data(), 1); // Squared L2 distance
			if (dist > max_distance) {
				max_distance = dist;
			}
		}
	}

	return max_distance;
}

std::vector<float> computeCentroid(const float* data, size_t numVectors, size_t dimensionality) {
	std::vector<float> centroid(dimensionality, 0.0f);
	for (size_t i = 0; i < numVectors; i++) {
		for (size_t j = 0; j < dimensionality; j++) {
			centroid[j] += data[i * dimensionality + j];
		}
	}
	for (size_t j = 0; j < dimensionality; j++) {
		centroid[j] /= numVectors;
	}
	return centroid;
}

float computeMaxDistance(const float* data, size_t numVectors, size_t dimensionality, const std::vector<float>& centroid) {
	float max_distance = 0.0f;
	for (size_t i = 0; i < numVectors; i++) {
		float dist = 0.0f;
		for (size_t j = 0; j < dimensionality; j++) {
			float diff = data[i * dimensionality + j] - centroid[j];
			dist += diff * diff;
		}
		dist = std::sqrt(dist);
		if (dist > max_distance) {
			max_distance = dist;
		}
	}
	return max_distance;
}

float computeConservativeMaxDistance(const float* data, size_t numVectors, size_t dimensionality) {
	// Sample a few pairs of points to estimate the maximum distance
	const size_t num_samples = 1000; // Adjust based on dataset size
	float max_distance = 0.0f;

	for (size_t i = 0; i < num_samples; i++) {
		size_t idx1 = rand() % numVectors;
		size_t idx2 = rand() % numVectors;
		float dist = 0.0f;
		for (size_t j = 0; j < dimensionality; j++) {
			float diff = data[idx1 * dimensionality + j] - data[idx2 * dimensionality + j];
			dist += diff * diff;
		}
		dist = std::sqrt(dist);
		if (dist > max_distance) {
			max_distance = dist;
		}
	}

	// Add a safety margin (e.g., 10%) to account for outliers
	return max_distance * 1.1f;
}

struct SearchIndex {
	size_t dimensionality;
	size_t numVectors;
	std::shared_ptr<std::map<size_t, std::string>> idToMd5;
	std::shared_ptr<faiss::Index> index;
	float max_l2_distance_approx = 0.0f;
};

typedef std::function<void(midikraft::PatchHolder const& patch, float* position, size_t capacity, size_t& out_dimensionality)> PatchFeatureVector;

class ExactSimilaritySearch {
public:

	ExactSimilaritySearch(midikraft::PatchDatabase& db) : db_(db) {
	}

	~ExactSimilaritySearch() = default;

	void buildIndexForSynth(std::shared_ptr<midikraft::Synth> synth, PatchFeatureVector featureVector, ProgressHandler *progressHandler) {
		progressHandler->setProgressPercentage(0.0);
		progressHandler->setMessage("Loading all patches into memory...");
		// Load all patches from the database for now
		midikraft::PatchFilter filter({synth});
		auto patches = db_.getPatches(filter, 0, -1);

		if (patches.size() == 0) {
			spdlog::error("No patches loaded from database, can't build similarity index!");
			return;
		}

		// Need to calculate a feature vector to glean the dimensionality
		size_t dimensionality = 0;
		featureVector(patches[0], nullptr, 0, dimensionality);

		// Now, we build two indices: The first is just integer to md5,
		// the second is the vector index of dimensionality equal to patch size
		// In this instance, we are just using the bytes (non blanked) of the patch
		// so a different name will already make a big difference.
		auto id_map = std::make_shared<std::map<size_t, std::string>>();
		progressHandler->setMessage("Calculating feature vectors for patches...");
		//auto newIndex = std::make_shared<faiss::IndexFlatL2>(dimensionality);
		// This matrix needs a lot of memory
		float* xb = new float[dimensionality * patches.size()];
		size_t write_pos = 0;
		for (size_t i = 0; i < patches.size(); i++) {
			id_map->emplace(i, patches[i].md5());
			size_t changed_dim = 0;
			featureVector(patches[i], &xb[write_pos], dimensionality, changed_dim);
			write_pos += dimensionality;			
			progressHandler->setProgressPercentage(i / static_cast<double>(patches.size()));
			if (progressHandler->shouldAbort()) {
				spdlog::info("Calculation of similarity index interrupted");
				return;
			}
		}

		// Copy raw data before normalization
		float* xb_raw = new float[dimensionality * patches.size()];
		std::memcpy(xb_raw, xb, dimensionality * patches.size() * sizeof(float));

		float max_distance = computeTrueMaxDistance(xb_raw, patches.size(), dimensionality);
		spdlog::info("Computed true max distance as {:.4f}", max_distance);

		// Compute centroid
		auto centroid = computeCentroid(xb_raw, patches.size(), dimensionality);

		// Compute approximate max L2 distance
		auto max_distance_centroid = computeMaxDistance(xb_raw, patches.size(), dimensionality, centroid);
		spdlog::info("Computed centroid max distance as {:.4f}", max_distance_centroid);

		// Compute conservative max L2 distance
		auto max_distance_conservative = computeConservativeMaxDistance(xb_raw, patches.size(), dimensionality);
		spdlog::info("Computed conservative max distance as {:.4f}", max_distance_conservative);

		// Normalize the data for IP
		normalizeVectors(xb, patches.size(), dimensionality);

		// Create IP index with normalized data
		auto ipIndex = std::make_shared<faiss::IndexFlatIP>(dimensionality);
		ipIndex->add(patches.size(), xb);
		indexes_.emplace(std::make_pair(synth->getName(), SimilarityMetric::IP), SearchIndex{ dimensionality, patches.size(), id_map, ipIndex });

		// Create L2 index with raw data
		auto l2Index = std::make_shared<faiss::IndexFlatL2>(dimensionality);
		l2Index->add(patches.size(), xb_raw);
		indexes_.emplace(std::make_pair(synth->getName(), SimilarityMetric::L2), SearchIndex{ dimensionality, patches.size(), id_map, l2Index, max_distance_centroid });

		// Cleanup
		delete[] xb;
		delete[] xb_raw;
	}

	bool hasIndex(std::shared_ptr<midikraft::Synth> synth, SimilarityMetric metric) {
		return indexes_.find(std::make_pair(synth->getName(), metric)) != indexes_.end();
	}

	std::vector<std::pair<float, std::string>> searchNeighbours(midikraft::PatchHolder const& examplePatch, int k, PatchFeatureVector featureVector, SimilarityMetric metric, float distance_cutoff) {
		auto index = indexes_.find(std::make_pair(examplePatch.smartSynth()->getName(), metric));
		if (index != indexes_.end()) {
			auto searchIndex = index->second;

			std::vector<float> features(searchIndex.dimensionality);
			size_t dim = searchIndex.dimensionality;
			featureVector(examplePatch, features.data(), searchIndex.dimensionality, dim);
			std::vector<float> distances(k);
			std::vector<faiss::idx_t> labels(k);

			switch (metric) {
			case SimilarityMetric::L2:
				spdlog::info("Searching using L2 metric");
				searchIndex.index->search(1, features.data(), k, distances.data(), labels.data());
				break;
			case SimilarityMetric::IP:
				spdlog::info("Searching using IP metric");
				normalizeVectors(features.data(), 1, searchIndex.dimensionality);
				searchIndex.index->search(1, features.data(), k, distances.data(), labels.data());
				break;
			}

			// The result is a list of md5s 
			std::vector<std::pair<float, std::string>> result;
			int i = 0;
			for (auto r : labels) {
				if (r != -1) {
					// Apply cutoff based on metric
					float similarity = 0.0f; // Need to get it into range 0..1 with 1 being identical
					switch (metric) {
					case SimilarityMetric::L2:
						similarity = 1.0f - (std::sqrt(distances[i]) / searchIndex.max_l2_distance_approx);
						similarity = std::max(0.0f, std::min(1.0f, similarity)); // Clamp to [0, 1]
						break;
					case SimilarityMetric::IP:
						similarity = (std::sqrt(distances[i]) + 1.0f) / 2.0f; // Map [-1, 1] → [0, 1]
						break;
					}
					if (similarity >= distance_cutoff) {
						spdlog::info("distance {} with similarity {} is good for cutoff {}", std::sqrt(distances[i]), similarity, distance_cutoff);
						result.emplace_back(similarity, searchIndex.idToMd5->at(r));
					}
					else {
						spdlog::info("distance {} with similarity {} did not meet cutoff criteria of {}", std::sqrt(distances[i]), similarity, distance_cutoff);
					}
				}
				i++;
			}
			return result;
		}
		else {
			spdlog::error("program error - no index whas been created or found to search for similar patches for synth '{}'", examplePatch.smartSynth()->getName());
			return {};
		}
	}


private:
	midikraft::PatchDatabase& db_;
	std::map<std::pair<std::string, SimilarityMetric>, SearchIndex> indexes_;
};

// PIMPL https://stackoverflow.com/questions/9954518/stdunique-ptr-with-an-incomplete-type-wont-compile
void PatchSimilarity::pimpl_deleter::operator()(ExactSimilaritySearch* ptr) const { delete ptr; }


PatchSimilarity::PatchSimilarity(midikraft::PatchDatabase& db) : db_(db) {
	impl_.reset(new ExactSimilaritySearch(db));
}


void patchDataAsFeatureVector(midikraft::PatchHolder const& patch, float* matrix, size_t capacity, size_t& out_dimensionality) {
	auto const& data = patch.patch()->data();
	out_dimensionality = data.size();
	if (matrix != nullptr) {
		size_t j = 0;
		for (uint8 const& value : data) {
			if (j >= capacity) {
				// Varying patch data size. TODO
				return;
			}
			matrix[j++] = static_cast<float>(value);
		}
	}
}

void featuresFromParameters(midikraft::PatchHolder const& patch, float* matrix, size_t capacity, size_t& out_dimensionality) {
	auto modernParameters = midikraft::Capability::hasCapability<midikraft::SynthParametersCapability>(patch.smartSynth());
	if (modernParameters) {
		auto featureVector = modernParameters->createFeatureVector(patch.patch());
		out_dimensionality = featureVector.size();
		if (matrix != nullptr) {
			size_t last_element = std::min(featureVector.size(), capacity);
			std::copy(featureVector.cbegin(), featureVector.cbegin() + last_element, matrix);
		}
	}
}

class BuildFeatureIndexWindow : public ProgressHandlerWindow {
public:
	BuildFeatureIndexWindow(std::shared_ptr<midikraft::Synth> synth
		, ExactSimilaritySearch &search
		, PatchFeatureVector featureVector
	) 
		: ProgressHandlerWindow("Creating index", fmt::format("Building the in-memory index for all patches of the {}", synth->getName())),
		synth_(synth), search_(search), featureVector_(featureVector)
	{
	}

	virtual void run() override {
		search_.buildIndexForSynth(synth_, featureVector_, this);
	}

private:
	std::shared_ptr<midikraft::Synth> synth_;
	ExactSimilaritySearch& search_;
	PatchFeatureVector featureVector_;
};

std::vector<midikraft::PatchHolder> PatchSimilarity::findSimilarPatches(midikraft::PatchHolder const& examplePatch, int k, SimilarityMetric metric, float distance_cutoff) {
	auto synth = examplePatch.smartSynth();
	bool hasModernParameters = midikraft::Capability::hasCapability<midikraft::SynthParametersCapability>(synth) != nullptr;

	if (!impl_->hasIndex(synth, metric)) {
		// Now, we need to build an index of all patches for this synth, should it not exist yet
		BuildFeatureIndexWindow progressWindow(synth, *impl_, hasModernParameters ? featuresFromParameters : patchDataAsFeatureVector);
		if (!progressWindow.runThread()) {
			// User abort
			return {};
		}
	}

	// And then call search
	auto neighbours = impl_->searchNeighbours(examplePatch, k, hasModernParameters ? featuresFromParameters : patchDataAsFeatureVector, metric, distance_cutoff);

	std::vector<midikraft::PatchHolder> result;
	for (auto const& neighbour : neighbours) {
		if (!db_.getSinglePatch(examplePatch.smartSynth(), neighbour.second, result)) {
			spdlog::error("Failed to load patch with md5 {} from database, outdated index?", neighbour.second);
		}
		else {
			spdlog::info("Next neighbour: {} at {:.4f}", result.back().name(), neighbour.first);
		}
	}
	return result;
}
