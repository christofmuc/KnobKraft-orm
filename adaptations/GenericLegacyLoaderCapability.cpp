/*
   Copyright (c) 2026 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "GenericLegacyLoaderCapability.h"

#include "GenericAdaptation.h"
#include "GenericPatch.h"

#include "JuceHeader.h"

#ifdef _MSC_VER
#pragma warning ( push )
#pragma warning ( disable: 4100 )
#endif
#include <pybind11/embed.h>
#include <pybind11/stl.h>
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include <algorithm>
#include <cctype>
#include <set>
#include <stdexcept>
#include <utility>

namespace py = pybind11;

namespace knobkraft {

	namespace {
		std::string normalizeExtension(std::string extension) {
			auto isWhitespace = [](unsigned char c) { return std::isspace(c) != 0; };
			extension.erase(extension.begin(), std::find_if(extension.begin(), extension.end(), [&](unsigned char c) { return !isWhitespace(c); }));
			extension.erase(std::find_if(extension.rbegin(), extension.rend(), [&](unsigned char c) { return !isWhitespace(c); }).base(), extension.end());
			if (extension.empty()) {
				return {};
			}

			if (extension == "*") {
				return extension;
			}

			if (extension.rfind("*.", 0) == 0 || extension.rfind("*", 0) == 0) {
				extension = extension.substr(1);
			}
			if (extension.empty()) {
				return {};
			}
			if (extension[0] != '.') {
				extension = "." + extension;
			}
			std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return extension;
		}

		void logLegacyExtensionCacheDisabled(GenericAdaptation* adaptation) {
			std::runtime_error ex("Legacy extension cache initialization failed; extension support checks are disabled for this adaptation instance.");
			adaptation->logAdaptationError(kLegacyLoadSupportedExtensions, ex);
		}
	}

	void GenericLegacyLoaderCapability::ensureCachedNormalizedExtensions()
	{
		std::call_once(cachedNormalizedExtensionsInitOnce_, [this]() {
			py::gil_scoped_acquire acquire;
			try {
				py::object result = me_->callMethod(kLegacyLoadSupportedExtensions);
				std::vector<std::string> extensions = result.cast<std::vector<std::string>>();
				std::vector<std::string> normalizedExtensions;
				std::string extensionPatterns;
				std::set<std::string> uniqueNormalized;
				std::set<std::string> uniquePatterns;
				for (auto const& extension : extensions) {
					auto normalized = normalizeExtension(extension);
					if (normalized.empty()) {
						continue;
					}
					uniqueNormalized.insert(normalized);
					if (normalized == "*") {
						uniquePatterns.insert("*");
					}
					else {
						uniquePatterns.insert("*" + normalized);
					}
				}

				normalizedExtensions.assign(uniqueNormalized.begin(), uniqueNormalized.end());

				for (auto const& pattern : uniquePatterns) {
					if (!extensionPatterns.empty()) {
						extensionPatterns += ";";
					}
					extensionPatterns += pattern;
				}

				cachedNormalizedExtensions_ = std::move(normalizedExtensions);
				cachedFileExtensionPatterns_ = std::move(extensionPatterns);
			}
			catch (py::error_already_set& ex) {
				cachedInitFailed_ = true;
				me_->logAdaptationError(kLegacyLoadSupportedExtensions, ex);
				ex.restore();
			}
			catch (std::exception& ex) {
				cachedInitFailed_ = true;
				me_->logAdaptationError(kLegacyLoadSupportedExtensions, ex);
			}
		});
	}

	std::string GenericLegacyLoaderCapability::additionalFileExtensions()
	{
		ensureCachedNormalizedExtensions();
		if (cachedInitFailed_) {
			logLegacyExtensionCacheDisabled(me_);
			return {};
		}
		return cachedFileExtensionPatterns_;
	}

	bool GenericLegacyLoaderCapability::supportsExtension(std::string const& filename)
	{
		auto fileExtension = juce::File(filename).getFileExtension().toStdString();
		std::transform(fileExtension.begin(), fileExtension.end(), fileExtension.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		ensureCachedNormalizedExtensions();
		if (cachedInitFailed_) {
			logLegacyExtensionCacheDisabled(me_);
			return false;
		}
		for (auto const& normalized : cachedNormalizedExtensions_) {
			if (normalized == "*") {
				return true;
			}
			if (fileExtension == normalized) {
				return true;
			}
		}
		return false;
	}

	midikraft::TPatchVector GenericLegacyLoaderCapability::load(std::string const& filename, std::vector<uint8> const& fileContent)
	{
		py::gil_scoped_acquire acquire;
		midikraft::TPatchVector patches;

		try {
			std::vector<int> data(fileContent.begin(), fileContent.end());
			py::object result = me_->callMethod(kLoadPatchesFromLegacyData, data, filename);
			auto patchList = result.cast<std::vector<std::vector<int>>>();

			for (auto const& patchBytes : patchList) {
				auto patchType = GenericPatch::PROGRAM_DUMP;
				if (me_->pythonModuleHasFunction(kIsEditBufferDump)) {
					try {
						if (me_->callMethod(kIsEditBufferDump, patchBytes).cast<bool>()) {
							patchType = GenericPatch::EDIT_BUFFER;
						}
					}
					catch (py::error_already_set& ex) {
						me_->logAdaptationError(kIsEditBufferDump, ex);
						ex.restore();
					}
					catch (std::exception& ex) {
						me_->logAdaptationError(kIsEditBufferDump, ex);
					}
				}
				if (patchType == GenericPatch::PROGRAM_DUMP && me_->pythonModuleHasFunction(kIsSingleProgramDump)) {
					try {
						// Intentionally only used as an optional validation/consistency hook.
						ignoreUnused(me_->callMethod(kIsSingleProgramDump, patchBytes).cast<bool>());
					}
					catch (py::error_already_set& ex) {
						me_->logAdaptationError(kIsSingleProgramDump, ex);
						ex.restore();
					}
					catch (std::exception& ex) {
						me_->logAdaptationError(kIsSingleProgramDump, ex);
					}
				}

				try {
					auto patchData = GenericAdaptation::intVectorToByteVector(patchBytes);
					patches.push_back(std::make_shared<GenericPatch>(me_, me_->adaptation_module, patchData, patchType));
				}
				catch (py::error_already_set& ex) {
					me_->logAdaptationError(kLoadPatchesFromLegacyData, ex);
					ex.restore();
				}
				catch (std::exception& ex) {
					me_->logAdaptationError(kLoadPatchesFromLegacyData, ex);
				}
			}
		}
		catch (py::error_already_set& ex) {
			me_->logAdaptationError(kLoadPatchesFromLegacyData, ex);
			ex.restore();
		}
		catch (std::exception& ex) {
			me_->logAdaptationError(kLoadPatchesFromLegacyData, ex);
		}
		return patches;
	}

}
