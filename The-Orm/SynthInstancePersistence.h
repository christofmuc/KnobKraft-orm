/*
   Copyright (c) 2026 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "ConfiguredSynthInstance.h"

#include <memory>
#include <string>

namespace midikraft {
	class SimpleDiscoverableDevice;
}

class SynthInstancePersistence {
public:
	static constexpr char const* CONFIGURED_SYNTHS_SETTING = "ConfiguredSynthInstances";

	// Restores the registry snapshot. Invalid state is reported and retained in
	// Settings; callers must not silently replace it with a new configuration.
	[[nodiscard]] static bool restore(midikraft::session::ConfiguredSynthInstanceRegistry& registry);

	// configurationSlotKey identifies the application's configuration slot, not
	// a synth model. Existing code supplies one model-backed slot; future support
	// for duplicate models must supply a distinct key for each slot.
	[[nodiscard]] static bool registerDevice(
		midikraft::session::ConfiguredSynthInstanceRegistry& registry,
		std::shared_ptr<midikraft::SimpleDiscoverableDevice> const& device,
		std::string const& configurationSlotKey);

	[[nodiscard]] static bool updateDevice(
		midikraft::session::ConfiguredSynthInstanceRegistry& registry,
		std::shared_ptr<midikraft::SimpleDiscoverableDevice> const& device);

	[[nodiscard]] static bool save(midikraft::session::ConfiguredSynthInstanceRegistry const& registry);

private:
	static bool configurationStateValid_;
	[[nodiscard]] static std::string instanceIdSetting(std::string const& configurationSlotKey);
	[[nodiscard]] static midikraft::session::ConfiguredSynthInstance snapshot(
		std::shared_ptr<midikraft::SimpleDiscoverableDevice> const& device);
};
