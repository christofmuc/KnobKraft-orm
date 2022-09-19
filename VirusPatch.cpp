/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "VirusPatch.h"

#include <boost/format.hpp>
#include <locale>
#include <codecvt>

namespace midikraft {

	std::vector<std::string> kVirusCategories = { "Off", "Lead", "Bass", "Pad", "Decay", "Pluck", "Acid", "Classic", "Arpeggiator", "Effects", "Drums", "Percussion", "Input", "Vocoder", "Favourite1", "Favourite2", "Favourite3" };

	// C++ 11 has u32string, which allows me to access any character with the character index from the Virus' sysex data. The arrows are not rendered by JUCE, however
	std::u32string virus_codepage = U" !\"  #&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[¥]^_`abcdefghijklmnopqrstuvwxyz{|}→←";
	//                                       &' )   -./0         :    ?@A                          ¥                                }
	// Second line lists verified characters

	// Now, this gets more ugly the longer I look at it. The whole uconvert function down there is also deprecated from C++ 17, so I am sure I will come back
#ifdef _MSC_VER
	typedef int32_t CHAR32_T_FIX;
#else
	typedef char32_t CHAR32_T_FIX;
#endif

	// Constructing the converter is really slow, so don't do this in a loop!
	std::wstring_convert<std::codecvt_utf8<CHAR32_T_FIX>, CHAR32_T_FIX> convert;

	// Sigh
	// https://stackoverflow.com/questions/32055357/visual-studio-c-2015-stdcodecvt-with-char16-t-or-char32-t
	std::string utf32_to_utf8(char32_t utf32_char)
	{
		std::u32string utf32_string;
		utf32_string.push_back(utf32_char);
		auto p = reinterpret_cast<const CHAR32_T_FIX *>(utf32_string.data());
		return convert.to_bytes(p, p + utf32_string.size());
	}


	VirusPatch::VirusPatch(Synth::PatchData const &data, MidiProgramNumber place) : Patch(PATCH_VIRUS_B, data), place_(place)
	{
		jassert(data.size() == 256);
	}

	std::string VirusPatch::name() const
	{
		std::string result;
		for (int i = index(PageB, 112); i < index(PageB, 122); i++) {
			// Character lookup
			uint8 charIndex = data()[i];
			if (charIndex >= 32 && charIndex <= 32 + virus_codepage.length()) {
				result = result + utf32_to_utf8(virus_codepage[charIndex - 32]); // Could be more than one character
			}
			else {
				// This does happen when you e.g. import an uninitialized bank like I did after I fixed the capacitor in mine, and the RAM content is gone
				result.push_back('?');
			}
		}
		return result;
	}

	void VirusPatch::setName(std::string const &name)
	{
		ignoreUnused(name);
		//TODO
	}

	bool VirusPatch::isDefaultName(std::string const &patchName) const
	{
		return patchName == "- Init -";
	}

	MidiProgramNumber VirusPatch::patchNumber() const
	{
		return place_;
	}

	int VirusPatch::index(Page page, int index)
	{
		// We have stored the A and the B pages in one vector
		return page * 128 + index;
	}

	bool VirusPatch::setTags(std::set<Tag> const &tags)
	{
		ignoreUnused(tags);
		throw std::logic_error("The method or operation is not implemented.");
	}

	std::set<Tag> VirusPatch::tags() const
	{
		std::set<Tag> result;
		size_t cat1 = data()[index(PageB, 123)];
		if (cat1 != 0) {
			if (cat1 < kVirusCategories.size()) {
				result.emplace(kVirusCategories[cat1]);
			}
			else {
				SimpleLogger::instance()->postMessage((boost::format("Found invalid category in Virus patch %s: %d") % name() % cat1).str());
			}
		}
		size_t cat2 = data()[index(PageB, 124)];
		if (cat2 != 0) {
			if (cat2 < kVirusCategories.size()) {
				result.emplace(kVirusCategories[cat2]);
			}
			else {
				SimpleLogger::instance()->postMessage((boost::format("Found invalid secondary category in Virus patch %s: %d") % name() % cat2).str());
			}
		}
		return result;
	}

}
