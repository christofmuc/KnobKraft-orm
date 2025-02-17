/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "Patch.h"

#include "GenericAdaptation.h"

#include "Capability.h"
#include "StoredPatchNameCapability.h"
#include "LayeredPatchCapability.h"
#include "StoredTagCapability.h"

#ifdef _MSC_VER
#pragma warning ( push )
#pragma warning ( disable: 4100 )
#endif
#include <pybind11/embed.h>
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include <fmt/format.h>
#include <spdlog/spdlog.h>

namespace knobkraft {

	class GenericPatch;

	class GenericStoredPatchNameCapability : public midikraft::StoredPatchNameCapability {
	public:
		GenericStoredPatchNameCapability(GenericPatch *me, GenericAdaptation const* adaptation) : me_(me), adaptation_(adaptation) {}
        virtual ~GenericStoredPatchNameCapability() = default;
		bool changeNameStoredInPatch(std::string const &name) override;
		std::string name() const override;

	private:
		GenericPatch *me_;
		GenericAdaptation const* adaptation_;
	};

	class GenericDefaultNameCapability : public midikraft::DefaultNameCapability {
	public:
		GenericDefaultNameCapability(std::shared_ptr<GenericPatch> me) : me_(me) {}
        virtual ~GenericDefaultNameCapability() = default;
		virtual bool isDefaultName(std::string const &patchName) const override;

	private:
		std::weak_ptr<GenericPatch> me_;
	};

	class GenericLayeredPatchCapability : public midikraft::LayeredPatchCapability {
	public:
		GenericLayeredPatchCapability(std::shared_ptr<GenericPatch> me) : me_(me) {}
        virtual ~GenericLayeredPatchCapability() = default;
		virtual LayerMode layerMode() const override;
		virtual int numberOfLayers() const override;
		virtual std::vector<std::string> layerTitles() const override;
		virtual std::string layerName(int layerNo) const override;
		virtual void setLayerName(int layerNo, std::string const& layerName) override;

	private:
		std::weak_ptr<GenericPatch> me_;
	};

	class GenericStoredTagCapability : public midikraft::StoredTagCapability
	{
	public:
		GenericStoredTagCapability(std::shared_ptr<GenericPatch> me) : me_(me) {}
        virtual ~GenericStoredTagCapability() = default;
		virtual bool setTags(std::set<midikraft::Tag> const& tags) override;
		virtual std::set<midikraft::Tag> tags() const override;

	private:
		std::weak_ptr<GenericPatch> me_;
	};

	class GenericPatch : public midikraft::DataFile, public std::enable_shared_from_this<GenericPatch>
	{
	public:
		enum DataType {
			PROGRAM_DUMP = 0,
			EDIT_BUFFER
		};

		GenericPatch(GenericAdaptation const *me, pybind11::module &adaptation_module, midikraft::Synth::PatchData const &data, DataType dataType);
        virtual ~GenericPatch() = default;

		bool pythonModuleHasFunction(std::string const &functionName) const;

		template <typename ... Args>
		pybind11::object callMethod(std::string const &methodName, Args& ... args) const {
			pybind11::gil_scoped_acquire acquire;
			if (!adaptation_) {
				return pybind11::none();
			}
			if (pybind11::hasattr(*adaptation_, methodName.c_str())) {
				try {
					auto result = adaptation_.attr(methodName.c_str())(args...);
					checkForPythonOutputAndLog();
					return result;
				}
				catch (pybind11::error_already_set &ex) {
					logAdaptationError(methodName.c_str(), ex);
					ex.restore();
					return pybind11::none();
				}
				catch (std::exception &ex) {
					logAdaptationError(methodName.c_str(), ex);
					return pybind11::none();
				}
				catch (...) {
					throw std::runtime_error("Unhandled exception");
				}
			}
			else {
				spdlog::error("Adaptation: method {} not found, fatal!", methodName);
				return pybind11::none();
			}
		}

		
		// For error handling
		void logAdaptationError(const char *methodName, std::exception &e) const;

	private:
		GenericAdaptation const *me_;
		pybind11::module &adaptation_;
	};


}

