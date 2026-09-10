/*
   Copyright (c) 2026 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "MidiLoopbackTest.h"

#include "MidiController.h"
#include "MidiHelpers.h"

#include <algorithm>

namespace midikraft {

	namespace {

		struct TestCase {
			std::string name;
			MidiMessage message;
		};

		std::vector<juce::uint8> rawBytes(MidiMessage const& message)
		{
			return { message.getRawData(), message.getRawData() + message.getRawDataSize() };
		}

		MidiMessage makeTestSysex(size_t payloadSize, juce::uint8 testNumber)
		{
			// 0x7d is the MIDI manufacturer ID reserved for educational/development use.
			std::vector<juce::uint8> payload(payloadSize);
			payload[0] = 0x7d;
			payload[1] = 'K';
			payload[2] = 'K';
			payload[3] = testNumber;

			juce::uint32 randomState = 0x4b4b0000u + testNumber;
			for (size_t index = 4; index < payload.size(); ++index) {
				randomState = randomState * 1664525u + 1013904223u;
				payload[index] = static_cast<juce::uint8>((randomState >> 16) & 0x7f);
			}
			return MidiHelpers::sysexMessage(payload);
		}

		std::vector<TestCase> createTestCases()
		{
			std::vector<TestCase> tests = {
				{ "Note On", MidiMessage::noteOn(16, 60, static_cast<juce::uint8>(100)) },
				{ "Note Off", MidiMessage::noteOff(16, 60, static_cast<juce::uint8>(64)) },
				{ "Control Change", MidiMessage::controllerEvent(16, 119, 73) },
				{ "Program Change", MidiMessage::programChange(16, 117) },
				{ "Pitch Bend", MidiMessage::pitchWheel(16, 0x2345) },
				{ "Polyphonic Aftertouch", MidiMessage::aftertouchChange(16, 60, 47) },
				{ "Channel Pressure", MidiMessage::channelPressureChange(16, 53) }
			};

			const std::vector<size_t> sysexSizes = { 16, 128, 1024, 4096, 16384 };
			juce::uint8 testNumber = 1;
			for (auto size : sysexSizes) {
				tests.push_back({ "SysEx payload " + std::to_string(size) + " bytes", makeTestSysex(size, testNumber++) });
			}
			return tests;
		}

		int timeoutFor(MidiMessage const& message)
		{
			// DIN MIDI transfers ten wire bits per byte at 31.25 kbit/s. Allow twice the
			// theoretical transfer duration plus driver and scheduling headroom.
			const int wireTimeMilliseconds = (message.getRawDataSize() * 10 * 1000 + 31249) / 31250;
			return std::max(1000, wireTimeMilliseconds * 2 + 500);
		}

		bool isCandidate(MidiMessage const& expected, MidiMessage const& received)
		{
			if (expected.isSysEx()) return received.isSysEx();
			if (received.isSysEx() || received.getRawDataSize() == 0) return false;
			return received.getRawData()[0] == expected.getRawData()[0];
		}

		int firstDifference(std::vector<juce::uint8> const& expected, std::vector<juce::uint8> const& received)
		{
			const auto commonSize = std::min(expected.size(), received.size());
			for (size_t index = 0; index < commonSize; ++index) {
				if (expected[index] != received[index]) return static_cast<int>(index);
			}
			return expected.size() == received.size() ? -1 : static_cast<int>(commonSize);
		}

		struct CallbackState {
			explicit CallbackState(juce::String inputIdentifier) : inputIdentifier(std::move(inputIdentifier)) {}

			juce::String inputIdentifier;
			CriticalSection lock;
			WaitableEvent receivedEvent;
			MidiMessage expected;
			std::vector<juce::uint8> received;
			std::vector<juce::uint8> partial;
			bool waiting = false;
		};

		class LoopbackRunner {
		public:
			LoopbackRunner(juce::MidiDeviceInfo const& input, juce::MidiDeviceInfo const& output,
				std::weak_ptr<ProgressHandler> progressHandler) :
				input_(input), output_(output), progressHandler_(progressHandler),
				callbackState_(std::make_shared<CallbackState>(input.identifier))
			{
			}

			MidiLoopbackTestReport run()
			{
				MidiLoopbackTestReport report;
				report.midiInput = input_;
				report.midiOutput = output_;

				auto controller = MidiController::instance();
				if (!controller->enableMidiInput(input_)) {
					report.error = "Could not open MIDI input " + input_.name.toStdString();
					return report;
				}

				auto midiOutput = controller->getMidiOutput(output_);
				if (!midiOutput->isValid()) {
					report.error = "Could not open MIDI output " + output_.name.toStdString();
					return report;
				}

				auto callbackState = callbackState_;
				controller->addMessageHandler(handler_, [callbackState](MidiInput* source, MidiMessage const& message) {
					if (source == nullptr || source->getDeviceInfo().identifier != callbackState->inputIdentifier) return;

					const ScopedLock lock(callbackState->lock);
					if (!callbackState->waiting || !isCandidate(callbackState->expected, message)) return;
					callbackState->received = rawBytes(message);
					callbackState->receivedEvent.signal();
				});
				controller->addPartialMessageHandler(handler_, [callbackState](MidiInput* source, const juce::uint8* data,
					int numBytesSoFar, double timestamp) {
					ignoreUnused(timestamp);
					if (source == nullptr || source->getDeviceInfo().identifier != callbackState->inputIdentifier
						|| data == nullptr || numBytesSoFar <= 0) return;

					const ScopedLock lock(callbackState->lock);
					if (!callbackState->waiting || !callbackState->expected.isSysEx()) return;
					callbackState->partial.assign(data, data + numBytesSoFar);
				});

				const auto tests = createTestCases();
				for (size_t index = 0; index < tests.size(); ++index) {
					if (shouldAbort()) {
						report.cancelled = true;
						break;
					}

					if (auto progress = progressHandler_.lock()) {
						progress->setMessage("Testing " + tests[index].name + "...");
					}
					report.results.push_back(runTest(tests[index], *midiOutput));
					if (report.results.back().status == MidiLoopbackTestStatus::Cancelled) {
						report.cancelled = true;
						break;
					}
					if (auto progress = progressHandler_.lock()) {
						progress->setProgressPercentage((index + 1.0) / tests.size());
					}
					Thread::sleep(20);
				}

				controller->removePartialMessageHandler(handler_);
				controller->removeMessageHandler(handler_);
				return report;
			}

		private:
			MidiLoopbackTestResult runTest(TestCase const& test, SafeMidiOutput& midiOutput)
			{
				MidiLoopbackTestResult result;
				result.name = test.name;
				result.expected = rawBytes(test.message);

				{
					const ScopedLock lock(callbackState_->lock);
					callbackState_->expected = test.message;
					callbackState_->received.clear();
					callbackState_->partial.clear();
					callbackState_->waiting = true;
					callbackState_->receivedEvent.reset();
				}

				const auto started = Time::getMillisecondCounter();
				midiOutput.sendMessageNow(test.message);
				const bool received = waitForMessage(timeoutFor(test.message));
				result.elapsedMilliseconds = static_cast<int>(Time::getMillisecondCounter() - started);

				{
					const ScopedLock lock(callbackState_->lock);
					callbackState_->waiting = false;
					result.received = callbackState_->received.empty() ? callbackState_->partial : callbackState_->received;
				}
				if (!result.received.empty()) result.firstDifferentByte = firstDifference(result.expected, result.received);

				if (shouldAbort()) {
					result.status = MidiLoopbackTestStatus::Cancelled;
				}
				else if (!received) {
					result.status = result.received.empty() ? MidiLoopbackTestStatus::TimedOut : MidiLoopbackTestStatus::Incomplete;
				}
				else {
					result.status = result.firstDifferentByte == -1 ? MidiLoopbackTestStatus::Passed : MidiLoopbackTestStatus::Mismatch;
				}
				return result;
			}

			bool waitForMessage(int timeoutMilliseconds)
			{
				const auto started = Time::getMillisecondCounter();
				while (static_cast<int>(Time::getMillisecondCounter() - started) < timeoutMilliseconds) {
					if (callbackState_->receivedEvent.wait(50)) return true;
					if (shouldAbort()) return false;
				}
				return false;
			}

			bool shouldAbort() const
			{
				if (auto progress = progressHandler_.lock()) return progress->shouldAbort();
				return false;
			}

			juce::MidiDeviceInfo input_;
			juce::MidiDeviceInfo output_;
			std::weak_ptr<ProgressHandler> progressHandler_;
			MidiController::HandlerHandle handler_ = MidiController::makeOneHandle();
			std::shared_ptr<CallbackState> callbackState_;
		};

	}

	bool MidiLoopbackTestReport::allPassed() const
	{
		return error.empty() && !cancelled && !results.empty()
			&& std::all_of(results.begin(), results.end(), [](MidiLoopbackTestResult const& result) {
				return result.status == MidiLoopbackTestStatus::Passed;
			});
	}

	MidiLoopbackTestReport MidiLoopbackTest::run(juce::MidiDeviceInfo const& midiInput,
		juce::MidiDeviceInfo const& midiOutput, std::weak_ptr<ProgressHandler> progressHandler)
	{
		return LoopbackRunner(midiInput, midiOutput, progressHandler).run();
	}

}
