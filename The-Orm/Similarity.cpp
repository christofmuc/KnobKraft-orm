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


struct SearchIndex {
	size_t dimensionality;
	size_t numVectors;
	std::shared_ptr<std::map<size_t, std::string>> idToMd5;
	std::shared_ptr<faiss::Index> index;
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

		normalizeVectors(xb, patches.size(), dimensionality);
		auto ipIndex = std::make_shared<faiss::IndexFlatIP>(dimensionality);
		ipIndex->add(patches.size(), xb);
		indexes_.emplace(std::make_pair(synth->getName(), SimilarityMetric::IP), SearchIndex{dimensionality, patches.size(), id_map, ipIndex});

		auto l2Index = std::make_shared<faiss::IndexFlatL2>(dimensionality);
		l2Index->add(patches.size(), xb);
		indexes_.emplace(std::make_pair(synth->getName(), SimilarityMetric::L2), SearchIndex{dimensionality, patches.size(), id_map, l2Index});
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
			normalizeVectors(features.data(), 1, searchIndex.dimensionality);
			std::vector<float> distances(k);
			std::vector<faiss::idx_t> labels(k);

			switch (metric) {
			case SimilarityMetric::L2:
				searchIndex.index->search(1, features.data(), k, distances.data(), labels.data());
				break;
			case SimilarityMetric::IP:
				searchIndex.index->search(1, features.data(), k, distances.data(), labels.data());
				break;
			}

			// The result is a list of md5s 
			std::vector<std::pair<float, std::string>> result;
			int i = 0;
			for (auto r : labels) {
				if (r != -1) {
					// Apply cutoff based on metric
					bool passes_cutoff = false;
					switch (metric) {
					case SimilarityMetric::L2:
						passes_cutoff = distances[i] < distance_cutoff;
						break;
					case SimilarityMetric::IP:
						passes_cutoff = distances[i] > distance_cutoff; // Higher is better
						break;
					}
					if (passes_cutoff) {
						result.emplace_back(distances[i], searchIndex.idToMd5->at(r));
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
