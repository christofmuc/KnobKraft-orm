#include "MidiLogPanel.h"

#include "LayoutConstants.h"

#include "Sysex.h"
#include "MidiController.h"

#include "spdlog/spdlog.h"

#include <regex>

MidiLogPanel::MidiLogPanel() {
	addAndMakeVisible(midiLogView_);
	sysexEntryLabel_.setText("Sysex entry", dontSendNotification);
	addAndMakeVisible(sysexEntryLabel_);

	sysexEntry_.onReturnKey = [this]() { send();  };
    sysexEntry_.setTextToShowWhenEmpty("enter sysex message to send, e.g. f0 43 22 01 00 00 f7, press return to send.", Colours::grey);
	addAndMakeVisible(sysexEntry_);

	sendSysex_.setButtonText("Send");
	sendSysex_.onClick = [this]() {
		send();
	};
	addAndMakeVisible(sendSysex_);
}


std::optional<std::vector<uint8>> parseSysexString(const juce::String& input)
{
    std::vector<uint8> sysexData;
    auto tokens = juce::StringArray::fromTokens(input, " ", "");

    std::regex hexRegex("^[0-9A-Fa-f]{1,2}$"); // Matches valid 1 or 2 digit hex numbers
    for (auto& token : tokens)
    {
        if (token.startsWithIgnoreCase("0x"))
            token = token.substring(2);  // Remove "0x" if present

        if (!std::regex_match(token.toStdString(), hexRegex))
        {
            spdlog::error("Error: Invalid hexadecimal token: {}", token.toStdString());
            return std::nullopt; // Invalid hex, return empty optional
        }

        sysexData.push_back(static_cast<uint8>(token.getHexValue32()));
    }

    return sysexData;  // Return parsed data if no errors
}

void MidiLogPanel::send() {
    auto result = parseSysexString(sysexEntry_.getText().trim());
    if (result.has_value()) {
        std::vector<uint8> entry = *result;
        if (entry.size() > 0) {
            if (entry[0] != 0xf0) {
                std::vector<uint8> fixed({0xf0});
                std::copy(entry.cbegin(), entry.cend(), std::back_inserter(fixed));
                entry = fixed;
            }
            if (entry.back() != 0xf7) {
                entry.push_back(0xf7);
            }
            sendToMidiOuts(Sysex::vectorToMessages(entry));
        }
    }
    else
    {
        spdlog::error("Parsing error, can't format sysex message to send");
    }
}

void MidiLogPanel::sendToMidiOuts(std::vector<MidiMessage> const& messages) {
    for (auto output : midikraft::MidiController::instance()->currentOutputs(false)) {
        auto midiOut = midikraft::MidiController::instance()->getMidiOutput(output);
        midiOut->sendBlockOfMessagesFullSpeed(messages);
    }
}

void MidiLogPanel::resized()
{
	auto area = getLocalBounds();

	auto sysexSendRow = area.removeFromBottom(LAYOUT_LINE_SPACING);
	sysexEntryLabel_.setBounds(sysexSendRow.removeFromLeft(LAYOUT_BUTTON_WIDTH).withTrimmedLeft(LAYOUT_INSET_SMALL));
	sendSysex_.setBounds(sysexSendRow.removeFromRight(LAYOUT_BUTTON_WIDTH + LAYOUT_INSET_NORMAL).withTrimmedLeft(LAYOUT_INSET_NORMAL));
	sysexEntry_.setBounds(sysexSendRow);

	midiLogView_.setBounds(area);
}