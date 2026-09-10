#pragma once

#include "Capability.h"
#include "PatchHolder.h"
#include "PatchTextCapability.h"

namespace patch_text {

	inline midikraft::PatchTextViews viewsFor(midikraft::PatchHolder const& patch) {
		auto capability = midikraft::Capability::hasCapability<midikraft::PatchTextCapability>(patch.synth());
		return capability && patch.patch() ? capability->getClearText(*patch.patch()) : midikraft::PatchTextViews{};
	}

	struct ComparisonView {
		std::string name, left, right;
	};

	inline std::vector<ComparisonView> commonViews(midikraft::PatchTextViews const& left, midikraft::PatchTextViews const& right) {
		std::vector<ComparisonView> result;
		for (auto const& l : left) {
			for (auto const& r : right) {
				if (l.first == r.first) {
					result.push_back({l.first, l.second, r.second});
					break;
				}
			}
		}
		return result;
	}

}
