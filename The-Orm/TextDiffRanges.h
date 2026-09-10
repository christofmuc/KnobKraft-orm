#pragma once

#include <juce_core/juce_core.h>
#include <vector>

namespace patch_text {

	struct DiffRanges {
		std::vector<juce::Range<int>> left, right;
	};

	inline std::vector<juce::String> linesWithEndings(juce::String const& text) {
		std::vector<juce::String> lines;
		auto cursor = text.getCharPointer();
		while (!cursor.isEmpty()) {
			auto start = cursor;
			while (!cursor.isEmpty()) {
				auto c = cursor.getAndAdvance();
				if (c == '\n') break;
				if (c == '\r') {
					if (*cursor == '\n') ++cursor;
					break;
				}
			}
			lines.emplace_back(start, cursor);
		}
		return lines;
	}

	inline DiffRanges diffRanges(juce::String const& left, juce::String const& right) {
		DiffRanges result;
		auto append = [&](juce::String const& a, juce::String const& b, int leftStart, int rightStart) {
			int offset = 0;
			// TextDiff positions address the progressively edited string. Translate
			// deletions back to the original; insertions already address the target.
			juce::TextDiff difference(a, b);
			for (auto const& change : difference.changes) {
				if (change.isDeletion()) {
					auto start = leftStart + change.start - offset;
					result.left.emplace_back(start, start + change.length);
					offset -= change.length;
				}
				else {
					auto length = change.insertedText.length();
					result.right.emplace_back(rightStart + change.start, rightStart + change.start + length);
					offset += length;
				}
			}
		};

		auto leftLines = linesWithEndings(left);
		auto rightLines = linesWithEndings(right);
		if (leftLines.size() == rightLines.size()) {
			// Stable rows are usual for patch dumps. Diff them individually to avoid
			// TextDiff's coarse fallback on long texts with distant small changes.
			int leftStart = 0, rightStart = 0;
			for (size_t i = 0; i < leftLines.size(); ++i) {
				append(leftLines[i], rightLines[i], leftStart, rightStart);
				leftStart += leftLines[i].length();
				rightStart += rightLines[i].length();
			}
		}
		else {
			// Let JUCE find insertions/deletions when the number of rows changes.
			append(left, right, 0, 0);
		}
		return result;
	}

}
