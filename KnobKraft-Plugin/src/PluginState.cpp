/*
   Copyright (c) 2026 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "PluginState.h"

#include <utility>

namespace knobkraft::recall {
	using midikraft::session::CodecErrorCode;
	using midikraft::session::CodecLimits;
	using midikraft::session::RecallPolicy;
	using midikraft::session::SessionManifest;
	using midikraft::session::SessionManifestCodec;
	using midikraft::session::SessionPatch;
	using midikraft::session::SessionPatchCodec;
	using midikraft::session::SessionSound;

	namespace {
		std::vector<std::uint8_t> bytes(std::string const& text) {
			return { text.begin(), text.end() };
		}

		std::shared_ptr<StateSnapshot const> encodedSnapshot(SessionManifest manifest) {
			auto encoded = SessionManifestCodec::encode(manifest);
			if (!encoded) {
				return std::make_shared<StateSnapshot const>(StateSnapshot {
					std::move(manifest),
					{},
					StateDecodeError { encoded.error().code, encoded.error().path, encoded.error().message, false }
				});
			}
			return std::make_shared<StateSnapshot const>(StateSnapshot { std::move(manifest), bytes(encoded.value()), std::nullopt });
		}
	}

	PluginState::PluginState(SessionManifest initialManifest)
		: snapshot_(encodedSnapshot(std::move(initialManifest))) {
	}

	std::shared_ptr<StateSnapshot const> PluginState::snapshot() const noexcept {
		return snapshot_.load(std::memory_order_acquire);
	}

	std::vector<std::uint8_t> PluginState::serialize() const {
		return snapshot()->serializedState;
	}

	bool PluginState::restore(std::span<std::uint8_t const> serializedState) noexcept {
		auto const current = snapshot();
		try {
			CodecLimits const limits;
			auto const canPreserve = serializedState.size() <= limits.maxDocumentBytes;

			if (!canPreserve) {
				snapshot_.store(std::make_shared<StateSnapshot const>(StateSnapshot {
					current->manifest,
					current->serializedState,
					StateDecodeError {
						CodecErrorCode::DocumentTooLarge,
						"$",
						"plugin state exceeds the maximum recoverable document size",
						false
					}
				}), std::memory_order_release);
				return false;
			}

			std::vector<std::uint8_t> raw(serializedState.begin(), serializedState.end());
			auto const text = std::string_view(reinterpret_cast<char const*>(raw.data()), raw.size());
			auto decoded = SessionManifestCodec::decode(text, limits);
			if (!decoded) {
				auto const& error = decoded.error();
				snapshot_.store(std::make_shared<StateSnapshot const>(StateSnapshot {
					current->manifest,
					std::move(raw),
					StateDecodeError { error.code, error.path, error.message, true }
				}), std::memory_order_release);
				return false;
			}

			snapshot_.store(std::make_shared<StateSnapshot const>(StateSnapshot {
				std::move(decoded).value(), std::move(raw), std::nullopt
			}), std::memory_order_release);
			return true;
		}
		catch (...) {
			// Preserve the existing immutable snapshot if memory allocation or an
			// unexpected library failure occurs at this host API boundary.
			return false;
		}
	}

	bool PluginState::replaceManifest(SessionManifest manifest) noexcept {
		try {
			auto next = encodedSnapshot(std::move(manifest));
			if (next->hasDecodeError()) {
				auto const current = snapshot();
				snapshot_.store(std::make_shared<StateSnapshot const>(StateSnapshot {
					current->manifest, current->serializedState, next->decodeError
				}), std::memory_order_release);
				return false;
			}
			snapshot_.store(std::move(next), std::memory_order_release);
			return true;
		}
		catch (...) {
			return false;
		}
	}

	SessionManifest PluginState::embeddedFixture(std::string pluginInstanceId) {
		SessionPatch patch;
		patch.adaptationId = "Oberheim Matrix 1000";
		patch.dataTypeId = "single-program";
		patch.name = "Warm Bass (embedded fixture)";
		patch.payload = { 0xf0, 0x01, 0x02, 0xf7 };
		auto fingerprint = SessionPatchCodec::fingerprint(patch);
		if (fingerprint) patch.fingerprint = fingerprint.value();

		SessionManifest manifest;
		manifest.pluginInstanceId = std::move(pluginInstanceId);
		manifest.instanceName = "KnobKraft Recall";
		manifest.binding.fallbackAdaptationId = patch.adaptationId;
		manifest.recallPolicy = RecallPolicy::Manual;
		manifest.selectedSoundId = "fixture-sound";
		manifest.sounds.push_back(SessionSound { "fixture-sound", std::move(patch) });
		return manifest;
	}

}
