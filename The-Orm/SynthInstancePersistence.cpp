/*
   Copyright (c) 2026 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "SynthInstancePersistence.h"

#include "AutoDetection.h"
#include "Capability.h"
#include "CustomProgramChangeCapability.h"
#include "EditBufferCapability.h"
#include "MidiController.h"
#include "ProgramDumpCapability.h"
#include "Settings.h"
#include "SimpleDiscoverableDevice.h"
#include "Synth.h"

#include <fmt/format.h>
#include <spdlog/spdlog.h>

namespace {
	std::optional<midikraft::session::MidiPortAssignment> portAssignment(juce::MidiDeviceInfo const& port) {
		if (port.name.isEmpty()) return std::nullopt;
		return midikraft::session::MidiPortAssignment { port.identifier.toStdString(), port.name.toStdString() };
	}

	std::optional<midikraft::session::MidiPortAssignment> legacyPortAssignment(std::string const& adaptationId, char const* trait) {
		auto const name = Settings::instance().get(fmt::format("{}-{}", adaptationId, trait));
		if (name.empty()) return std::nullopt;
		return midikraft::session::MidiPortAssignment { {}, name };
	}
}

bool SynthInstancePersistence::configurationStateValid_ = true;

bool SynthInstancePersistence::restore(midikraft::session::ConfiguredSynthInstanceRegistry& registry) {
	auto const persisted = Settings::instance().get(CONFIGURED_SYNTHS_SETTING);
	auto restored = registry.restore(persisted);
	if (!restored) {
		configurationStateValid_ = false;
		spdlog::error("Could not restore configured synth identities at {}: {}", restored.error().path, restored.error().message);
		return false;
	}
	configurationStateValid_ = true;
	return true;
}

bool SynthInstancePersistence::registerDevice(
	midikraft::session::ConfiguredSynthInstanceRegistry& registry,
	std::shared_ptr<midikraft::SimpleDiscoverableDevice> const& device,
	std::string const& configurationSlotKey)
{
	if (!configurationStateValid_ || !device || configurationSlotKey.empty()) return false;
	auto const idKey = instanceIdSetting(configurationSlotKey);
	std::optional<std::string> persistedId;
	if (Settings::instance().keyIsSet(idKey)) persistedId = Settings::instance().get(idKey);

	// Set an existing exact identity before loading MIDI settings so the new
	// UUID-keyed settings are preferred. A legacy slot has no ID and therefore
	// falls back to the previous name-keyed settings once, below.
	if (persistedId) device->setConfiguredSynthInstanceId(*persistedId);
	midikraft::AutoDetection::loadSettings(device.get());

	auto current = snapshot(device);
	if (!current.midiInput) current.midiInput = legacyPortAssignment(device->getName(), "input");
	if (!current.midiOutput) current.midiOutput = legacyPortAssignment(device->getName(), "output");
	auto registered = registry.registerInstance(persistedId, std::move(current));
	if (!registered) {
		spdlog::error("Could not register configured synth '{}': {}", device->getName(), registered.error().message);
		return false;
	}

	device->setConfiguredSynthInstanceId(registered.value());
	Settings::instance().set(idKey, registered.value());
	// This copies legacy MIDI settings into their new UUID-keyed location and
	// keeps future renames independent from the adaptation name.
	midikraft::AutoDetection::persistSetting(device.get());
	return updateDevice(registry, device) && save(registry);
}

bool SynthInstancePersistence::updateDevice(
	midikraft::session::ConfiguredSynthInstanceRegistry& registry,
	std::shared_ptr<midikraft::SimpleDiscoverableDevice> const& device)
{
	if (!configurationStateValid_ || !device || device->configuredSynthInstanceId().empty()) return false;
	auto current = snapshot(device);
	current.instanceId = device->configuredSynthInstanceId();
	return registry.updateInstance(current);
}

bool SynthInstancePersistence::save(midikraft::session::ConfiguredSynthInstanceRegistry const& registry) {
	if (!configurationStateValid_) return false;
	auto encoded = registry.serialize();
	if (!encoded) {
		spdlog::error("Could not save configured synth identities at {}: {}", encoded.error().path, encoded.error().message);
		return false;
	}
	Settings::instance().set(CONFIGURED_SYNTHS_SETTING, encoded.value());
	return true;
}

std::string SynthInstancePersistence::instanceIdSetting(std::string const& configurationSlotKey) {
	return fmt::format("ConfiguredSynthInstanceId-{}", configurationSlotKey);
}

midikraft::session::ConfiguredSynthInstance SynthInstancePersistence::snapshot(
	std::shared_ptr<midikraft::SimpleDiscoverableDevice> const& device)
{
	midikraft::session::ConfiguredSynthInstance result;
	result.instanceId = device->configuredSynthInstanceId();
	result.displayName = device->getName();
	result.adaptationId = device->getName();
	result.midiInput = portAssignment(device->midiInput());
	result.midiOutput = portAssignment(device->midiOutput());
	if (device->channel().isValid() && !device->channel().isOmni()) result.midiChannel = device->channel().toOneBasedInt();
	result.online = device->wasDetected();

	auto synth = std::dynamic_pointer_cast<midikraft::Synth>(device);
	if (synth) {
		result.capabilities.editBuffer = midikraft::Capability::hasCapability<midikraft::EditBufferCapability>(synth) != nullptr;
		result.capabilities.programDump = midikraft::Capability::hasCapability<midikraft::ProgramDumpCabability>(synth) != nullptr;
		result.capabilities.customProgramChange = midikraft::Capability::hasCapability<midikraft::CustomProgramChangeCapability>(synth) != nullptr;
		// Edit-buffer readback is the verification mechanism available in the
		// current capability model. Sending success alone is not verification.
		result.capabilities.verification = result.capabilities.editBuffer;
	}
	return result;
}
