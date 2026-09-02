/*
   Copyright (c) 2026 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "SessionCodecs.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace knobkraft::recall {

	struct StateDecodeError {
		midikraft::session::CodecErrorCode code;
		std::string path;
		std::string message;
		bool rawStatePreserved = false;
	};

	struct StateSnapshot {
		midikraft::session::SessionManifest manifest;
		std::vector<std::uint8_t> serializedState;
		std::optional<StateDecodeError> decodeError;

		[[nodiscard]] bool hasDecodeError() const noexcept { return decodeError.has_value(); }
	};

	class PluginState {
	public:
		explicit PluginState(midikraft::session::SessionManifest initialManifest);

		[[nodiscard]] std::shared_ptr<StateSnapshot const> snapshot() const noexcept;
		[[nodiscard]] std::vector<std::uint8_t> serialize() const;

		// A failed restore preserves the last valid manifest for display and keeps
		// bounded raw input verbatim so a later save cannot silently erase it.
		bool restore(std::span<std::uint8_t const> serializedState) noexcept;
		bool replaceManifest(midikraft::session::SessionManifest manifest) noexcept;

		[[nodiscard]] static midikraft::session::SessionManifest embeddedFixture(std::string pluginInstanceId);

	private:
		std::atomic<std::shared_ptr<StateSnapshot const>> snapshot_;
	};

}
