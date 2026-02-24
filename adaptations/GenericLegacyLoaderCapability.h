/*
   Copyright (c) 2026 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "LegacyLoaderCapability.h"

#include <mutex>
#include <string>
#include <vector>

namespace knobkraft {

	class GenericAdaptation;

	class GenericLegacyLoaderCapability : public midikraft::LegacyLoaderCapability {
	public:
		GenericLegacyLoaderCapability(GenericAdaptation* me) : me_(me) {}
		virtual ~GenericLegacyLoaderCapability() = default;

		virtual std::string additionalFileExtensions() override;
		virtual bool supportsExtension(std::string const& filename) override;
		virtual midikraft::TPatchVector load(std::string const& filename, std::vector<uint8> const& fileContent) override;

	private:
		void ensureCachedNormalizedExtensions();

		GenericAdaptation* me_;
		std::once_flag cachedNormalizedExtensionsInitOnce_;
		std::vector<std::string> cachedNormalizedExtensions_;
		std::string cachedFileExtensionPatterns_;
	};

}
