#include "Similarity.h"

#include "Capability.h"
#include "DetailedParametersCapability.h"
#include "ProgressHandlerWindow.h"

#include "faiss/IndexFlat.h"

#include <spdlog/spdlog.h>

struct SearchIndex {
	size_t dimensionality;
	size_t numVectors;
	std::shared_ptr<std::map<size_t, std::string>> idToMd5;
	std::shared_ptr<faiss::Index> index;
};

typedef std::function<void(midikraft::PatchHolder const& patch, float* position, size_t& d)> PatchFeatureVector;

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
		featureVector(patches[0], nullptr, dimensionality);

		// Now, we build two indices: The first is just integer to md5,
		// the second is the vector index of dimensionality equal to patch size
		// In this instance, we are just using the bytes (non blanked) of the patch
		// so a different name will already make a big difference.
		auto id_map = std::make_shared<std::map<size_t, std::string>>();
		progressHandler->setMessage("Calculating feature vectors for patches...");
		auto newIndex = std::make_shared<faiss::IndexFlatL2>(dimensionality);
		// This matrix needs a lot of memory
		float* xb = new float[dimensionality * patches.size()];
		size_t write_pos = 0;
		for (size_t i = 0; i < patches.size(); i++) {
			id_map->emplace(i, patches[i].md5());
			featureVector(patches[i], &xb[write_pos], dimensionality);
			write_pos += dimensionality;			
			progressHandler->setProgressPercentage(i / static_cast<double>(patches.size()));
			if (progressHandler->shouldAbort()) {
				spdlog::info("Calculation of similarity index interrupted");
				return;
			}
		}
		newIndex->add(patches.size(), xb);
		indexes_.emplace(synth->getName(), SearchIndex{ dimensionality, patches.size(), id_map, newIndex});
	}

	bool hasIndex(std::shared_ptr<midikraft::Synth> synth) {
		return indexes_.find(synth->getName()) != indexes_.end();
	}

	std::vector<std::string> searchNeighbours(midikraft::PatchHolder const& examplePatch, int k, PatchFeatureVector featureVector) {
		auto index = indexes_.find(examplePatch.smartSynth()->getName());
		if (index != indexes_.end()) {
			auto searchIndex = index->second;

			std::vector<float> features(searchIndex.dimensionality);
			featureVector(examplePatch, features.data(), searchIndex.dimensionality);
			std::vector<float> distances(k);
			std::vector<faiss::idx_t> labels(k);
			searchIndex.index->search(1, features.data(), k, distances.data(), labels.data());

			// The result is a list of md5s 
			std::vector<std::string> result;
			for (auto r : labels) {
				if (r != -1) {
					result.emplace_back(searchIndex.idToMd5->at(r));
				}
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
	std::map<std::string, SearchIndex> indexes_;
};

// PIMPL https://stackoverflow.com/questions/9954518/stdunique-ptr-with-an-incomplete-type-wont-compile
void PatchSimilarity::pimpl_deleter::operator()(ExactSimilaritySearch* ptr) const { delete ptr; }


PatchSimilarity::PatchSimilarity(midikraft::PatchDatabase& db) : db_(db) {
	impl_.reset(new ExactSimilaritySearch(db));
}


void patchDataAsFeatureVector(midikraft::PatchHolder const& patch, float* matrix, size_t &d) {
	auto const& data = patch.patch()->data();
	if (matrix != nullptr) {
		for (size_t j = 0; j < data.size(); j++) {
			matrix[j] = data[j];
		}
	}
	d = data.size();
}

void featuresFromParameters(midikraft::PatchHolder const& patch, float* matrix, size_t &d) {
	auto modernParameters = midikraft::Capability::hasCapability<midikraft::SynthParametersCapability>(patch.smartSynth());
	if (modernParameters) {
		auto featureVector = modernParameters->createFeatureVector(patch.patch());
		d = featureVector.size();
		if (matrix != nullptr) {
			std::copy(featureVector.cbegin(), featureVector.cend(), matrix);
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

std::vector<midikraft::PatchHolder> PatchSimilarity::findSimilarPatches(midikraft::PatchHolder const& examplePatch, int k) {
	if (!impl_->hasIndex(examplePatch.smartSynth())) {
		// Now, we need to build an index of all patches for this synth, should it not exist yet
		BuildFeatureIndexWindow progressWindow(examplePatch.smartSynth(), *impl_, featuresFromParameters);
		if (!progressWindow.runThread()) {
			// User abort
			return {};
		}
	}

	// And then call search
	auto neighbours = impl_->searchNeighbours(examplePatch, k, featuresFromParameters);

	std::vector<midikraft::PatchHolder> result;
	for (auto const& neighbour : neighbours) {
		if (!db_.getSinglePatch(examplePatch.smartSynth(), neighbour, result)) {
			spdlog::error("Failed to load patch with md5 {} from database, outdated index?", neighbour);
		}
		else {
			spdlog::info("Next neighbour: {}", result.back().name());
		}
	}
	return result;
}
