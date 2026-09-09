/*
   Copyright (c) 2026 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "GenericUploadHandshakeCapability.h"

#include <pybind11/embed.h>
#include <pybind11/stl.h>

#include <stdexcept>

namespace py = pybind11;

namespace knobkraft {

	bool GenericUploadHandshakeCapability::expectsUploadReply(const MidiMessage& sentMessage) const
	{
		if (!me_->pythonModuleHasFunction(kExpectsUploadReply)) return true;
		py::gil_scoped_acquire acquire;
		try {
			auto sent = me_->messageToVector(sentMessage);
			auto result = me_->callMethod(kExpectsUploadReply, sent);
			if (!py::isinstance<py::bool_>(result)) {
				throw std::runtime_error("expectsUploadReply must return a bool");
			}
			return result.cast<bool>();
		}
		catch (const py::error_already_set& ex) {
			std::runtime_error error(ex.what());
			me_->logAdaptationError(kExpectsUploadReply, error);
			throw error;
		}
	}

	midikraft::UploadHandshakeReply GenericUploadHandshakeCapability::isMessagePartOfUploadReply(
		const MidiMessage& message,
		const MidiMessage& sentMessage) const
	{
		py::gil_scoped_acquire acquire;
		try {
			auto incoming = me_->messageToVector(message);
			auto sent = me_->messageToVector(sentMessage);
			auto result = me_->callMethod(kIsPartOfUploadReply, incoming, sent);
			if (result.is_none()) return {};
			if (!py::isinstance<py::dict>(result)) {
				return { midikraft::UploadHandshakeReply::Status::ADAPTATION_ERROR, {}, "invalid_upload_reply", "isPartOfUploadReply must return None or a dict" };
			}

			auto dict = result.cast<py::dict>();
			auto statusKey = py::str("status");
			if (!dict.contains(statusKey) || !py::isinstance<py::str>(dict[statusKey])) {
				return { midikraft::UploadHandshakeReply::Status::ADAPTATION_ERROR, {}, "invalid_upload_reply", "Upload reply dict requires a string status" };
			}

			auto status = dict[statusKey].cast<std::string>();
			auto messagesKey = py::str("messages");
			if (status == "error" && dict.contains(messagesKey)) {
				return { midikraft::UploadHandshakeReply::Status::ADAPTATION_ERROR, {}, "invalid_upload_reply", "Upload error must not include response messages" };
			}

			std::vector<MidiMessage> response;
			if (dict.contains(messagesKey)) {
				auto bytes = dict[messagesKey].cast<std::vector<int>>();
				response = GenericAdaptation::vectorToMessages(bytes);
				if (!bytes.empty() && response.empty()) {
					return { midikraft::UploadHandshakeReply::Status::ADAPTATION_ERROR, {}, "invalid_upload_response", "Upload response does not contain a complete MIDI message" };
				}
			}

			if (status == "continue") {
				return { midikraft::UploadHandshakeReply::Status::CONTINUE, std::move(response) };
			}
			if (status == "accepted") {
				return { midikraft::UploadHandshakeReply::Status::ACCEPTED, std::move(response) };
			}
			if (status == "error") {
				auto codeKey = py::str("code");
				auto messageKey = py::str("message");
				if (!dict.contains(codeKey) || !dict.contains(messageKey)) {
					return { midikraft::UploadHandshakeReply::Status::ADAPTATION_ERROR, {}, "invalid_upload_reply", "Upload error requires code and message" };
				}
				auto code = dict[codeKey].cast<std::string>();
				auto description = dict[messageKey].cast<std::string>();
				if (code.empty() || description.empty()) {
					return { midikraft::UploadHandshakeReply::Status::ADAPTATION_ERROR, {}, "invalid_upload_reply", "Upload error code and message must not be empty" };
				}
				return { midikraft::UploadHandshakeReply::Status::DEVICE_ERROR, {}, std::move(code), std::move(description) };
			}
			return { midikraft::UploadHandshakeReply::Status::ADAPTATION_ERROR, {}, "invalid_upload_reply", "Unknown upload reply status: " + status };
		}
		catch (const std::exception& ex) {
			std::runtime_error error(ex.what());
			me_->logAdaptationError(kIsPartOfUploadReply, error);
			return { midikraft::UploadHandshakeReply::Status::ADAPTATION_ERROR, {}, "invalid_upload_reply", ex.what() };
		}
	}

	int GenericUploadHandshakeCapability::uploadReplyTimeoutMs() const
	{
		constexpr int defaultTimeoutMs = 5000;
		if (!me_->pythonModuleHasFunction(kMessageTimings)) return defaultTimeoutMs;
		py::gil_scoped_acquire acquire;
		try {
			auto result = me_->callMethod(kMessageTimings);
			if (!py::isinstance<py::dict>(result)) {
				throw std::runtime_error("messageTimings must return a dict");
			}
			auto dict = result.cast<py::dict>();
			auto key = py::str("uploadReplyTimeoutMs");
			if (!dict.contains(key)) return defaultTimeoutMs;
			if (!py::isinstance<py::int_>(dict[key])) {
				throw std::runtime_error("uploadReplyTimeoutMs must be an integer");
			}
			auto timeout = dict[key].cast<int>();
			if (timeout <= 0) throw std::runtime_error("uploadReplyTimeoutMs must be positive");
			return timeout;
		}
		catch (const py::error_already_set& ex) {
			std::runtime_error error(ex.what());
			me_->logAdaptationError(kMessageTimings, error);
			throw error;
		}
	}

}
