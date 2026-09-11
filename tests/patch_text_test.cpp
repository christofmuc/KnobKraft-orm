#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest/doctest.h"

#include "The-Orm/TextDiffRanges.h"
#include "The-Orm/PatchTextViews.h"
#include "The-Orm/PatchDiff.h"
#include "The-Orm/PatchTextBox.h"
#include "GenericAdaptation.h"
#include "Settings.h"
#include "test_helpers.h"

// Keep JUCE alive for the whole test run, including doctest reporters and
// queued adaptation error messages, rather than shutting it down in a test case.
int main(int argc, char** argv) {
	juce::ScopedJuceInitialiser_GUI gui;
	Settings::setSettingsID("KnobKraftPatchTextTests");
	const auto result = doctest::Context(argc, argv).run();
	// PropertiesFile owns a Timer, so destroy settings before JUCE's timer
	// infrastructure shuts down, just as the application does.
	Settings::shutdown();
	return result;
}

namespace {

juce::String withoutRanges(juce::String text, std::vector<juce::Range<int>> const& ranges) {
	int end = text.length();
	for (auto it = ranges.rbegin(); it != ranges.rend(); ++it) {
		CHECK(it->getStart() >= 0);
		CHECK(it->getEnd() <= end);
		end = it->getStart();
		text = text.replaceSection(it->getStart(), it->getLength(), {});
	}
	return text;
}

template<class T> std::vector<T*> children(juce::Component& component) {
	std::vector<T*> result;
	for (auto child : component.getChildren()) {
		if (auto match = dynamic_cast<T*>(child)) result.push_back(match);
	}
	return result;
}

class TextSynth : public test_helpers::DummySynth, public midikraft::PatchTextCapability {
public:
	TextSynth() : DummySynth("Text synth") {}
	midikraft::PatchTextViews getClearText(midikraft::DataFile const& patch) const override {
		if (patch.data().empty()) return {};
		return {{"Decoded", "  Head\r\n\r\n\tValue: " + std::to_string(patch.at(0)) + "  \n"}};
	}
};

}

TEST_CASE("text diffs use original character coordinates on both sides") {
	std::vector<std::pair<juce::String, juce::String>> cases = {
		{"", ""}, {"same\n", "same\n"}, {"", "new\n"}, {"old\n", ""},
		{"prefix VALUE suffix", "prefix NEW suffix"},
		{"abc common DELETE suffix", "INSERT abc common suffix"},
		{"one\n\nthree\n", "one\nTWO\n\nthree\n"},
		{"  one\r\n\tthree  \r\n", "  two\r\n\tthree  \r\n"},
		{juce::String::fromUTF8("\xc3\xa4\xf0\x9f\x8e\xb9 prefix OLD suffix"),
		 juce::String::fromUTF8("\xc3\xa4\xf0\x9f\x8e\xb9 prefix NEW suffix")}
	};
	for (auto const& texts : cases) {
		auto ranges = patch_text::diffRanges(texts.first, texts.second);
		CHECK(withoutRanges(texts.first, ranges.left) == withoutRanges(texts.second, ranges.right));
		if (texts.first == texts.second) {
			CHECK(ranges.left.empty());
			CHECK(ranges.right.empty());
		}
		else CHECK_FALSE((ranges.left.empty() && ranges.right.empty()));
	}
	auto ranges = patch_text::diffRanges("abc common DELETE suffix", "INSERT abc common suffix");
	REQUIRE(ranges.left.size() == 1);
	CHECK(ranges.left[0] == juce::Range<int>(11, 18));
	REQUIRE(ranges.right.size() == 1);
	CHECK(ranges.right[0] == juce::Range<int>(0, 7));
}

TEST_CASE("long dumps retain localized highlighting for distant byte changes") {
	juce::String left;
	for (int row = 0; row < 256; ++row) left += "0000 00 00 00 00 00 00 00 00\n";
	auto right = left.replaceSection(5, 2, "01").replaceSection(6008, 2, "02");
	auto ranges = patch_text::diffRanges(left, right);
	int highlighted = 0;
	for (auto const& range : ranges.left) highlighted += range.getLength();
	CHECK(highlighted <= 8);
	CHECK(withoutRanges(left, ranges.left) == withoutRanges(right, ranges.right));
}

TEST_CASE("view matching uses names and preserves both texts") {
	auto views = patch_text::commonViews({{"A", "left\r\n\r\n"}, {"B", " b "}, {"C", "c"}},
		{{"B", "\tright\n"}, {"A", "a\r\n"}, {"D", "d"}});
	REQUIRE(views.size() == 2);
	CHECK(views[0].name == "A");
	CHECK(views[0].left == "left\r\n\r\n");
	CHECK(views[0].right == "a\r\n");
	CHECK(views[1].name == "B");
	CHECK(views[1].right == "\tright\n");
}

TEST_CASE("Python patch text callback is optional and failures do not poison later calls") {
	struct Runtime {
		Runtime() { knobkraft::GenericAdaptation::startupGenericAdaptation(); }
		~Runtime() { knobkraft::GenericAdaptation::shutdownGenericAdaptation(); }
	} runtime;
	midikraft::DataFile patch(0, {0xf0, 0x42, 0x01, 0xf7});
	auto run = [&](std::string code) {
		return knobkraft::GenericAdaptation::fromBinaryCode("patch_text_fixture", "def name(): return 'Text fixture'\n" + code);
	};
	{
		auto adaptation = run("");
		REQUIRE(adaptation);
		CHECK(adaptation->getClearText(patch).empty());
	}
	{
		auto adaptation = run("def getClearText(message):\n"
			"    assert message == [240, 66, 1, 247]\n"
			"    message[0] = 0\n"
			"    return [('Decoded', '  \\u00e4\\r\\n\\r\\n\\tvalue  \\n'), ('Empty', '')]\n");
		REQUIRE(adaptation);
		auto views = adaptation->getClearText(patch);
		REQUIRE(views.size() == 2);
		CHECK(views[0].second == "  \xc3\xa4\r\n\r\n\tvalue  \n");
		CHECK(views[1].second.empty());
		CHECK(patch.at(0) == 0xf0);
	}
	for (auto code : {
		"def getClearText(message): raise ValueError('broken view')\n",
		"def getClearText(message): return [('A', 42)]\n",
		"def getClearText(message): return [('A', 'x'), ('A', 'y')]\n",
		"def getClearText(message): return [('', 'x')]\n",
		"def getClearText(message): return None\n"}) {
		auto adaptation = run(code);
		REQUIRE(adaptation);
		CHECK(adaptation->getClearText(patch).empty());
		CHECK(adaptation->getName() == "Text fixture");
		CHECK(adaptation->getClearText(patch).empty());
	}
}

TEST_CASE("comparison and sidebar retain custom whitespace through resize and view switching") {
	auto synth = std::make_shared<TextSynth>();
	auto left = test_helpers::makePatchHolder(synth, "Left", {1});
	auto right = test_helpers::makePatchHolder(synth, "Right", {2});
	PatchDiff comparison(synth.get(), left, right);
	auto selectors = children<juce::ComboBox>(comparison);
	REQUIRE(selectors.size() == 1);
	selectors[0]->setSelectedId(3, juce::sendNotificationSync);
	auto editors = children<juce::CodeEditorComponent>(comparison);
	REQUIRE(editors.size() == 2);
	CHECK(editors[0]->getDocument().getAllContent() == "  Head\r\n\r\n\tValue: 1  \n");
	CHECK(editors[1]->getDocument().getAllContent() == "  Head\r\n\r\n\tValue: 2  \n");
	comparison.setSize(200, 300);
	CHECK(editors[0]->getDocument().getAllContent() == "  Head\r\n\r\n\tValue: 1  \n");
	selectors[0]->setSelectedId(1, juce::sendNotificationSync);
	selectors[0]->setSelectedId(3, juce::sendNotificationSync);
	CHECK(editors[1]->getDocument().getAllContent() == "  Head\r\n\r\n\tValue: 2  \n");

	PatchTextBox sidebar({}, false);
	sidebar.fillTextBox(std::make_shared<midikraft::PatchHolder>(left));
	auto sidebarSelectors = children<juce::ComboBox>(sidebar);
	REQUIRE(sidebarSelectors.size() == 1);
	sidebarSelectors[0]->setSelectedId(3, juce::sendNotificationSync);
	auto sidebarEditors = children<juce::CodeEditorComponent>(sidebar);
	REQUIRE(sidebarEditors.size() == 1);
	CHECK(sidebarEditors[0]->getDocument().getAllContent() == "  Head\r\n\r\n\tValue: 1  \n");
	sidebar.setSize(120, 400);
	CHECK(sidebarEditors[0]->getDocument().getAllContent() == "  Head\r\n\r\n\tValue: 1  \n");
	sidebar.fillTextBox(std::make_shared<midikraft::PatchHolder>(right));
	CHECK(sidebarSelectors[0]->getSelectedId() == 3);
	CHECK(sidebarEditors[0]->getDocument().getAllContent() == "  Head\r\n\r\n\tValue: 2  \n");
	auto empty = test_helpers::makePatchHolder(synth, "Empty", {});
	sidebar.fillTextBox(std::make_shared<midikraft::PatchHolder>(empty));
	CHECK(sidebarSelectors[0]->getSelectedId() == 1);
	sidebar.fillTextBox(nullptr);
	CHECK(sidebarEditors[0]->getDocument().getAllContent() == "No patch active");
}
