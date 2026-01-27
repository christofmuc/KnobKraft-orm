#include "doctest/doctest.h"

#include "PatchDatabase.h"
#include "PatchList.h"
#include "test_helpers.h"

#include <algorithm>
#include <filesystem>
#include <functional>
#include <map>
#include <random>
#include <string>
#include <vector>

namespace {

using test_helpers::DummySynth;
using test_helpers::makePatchHolder;

class ScopedTempFile {
public:
	explicit ScopedTempFile(std::filesystem::path path) : path_(std::move(path)) {}
	~ScopedTempFile() {
		std::error_code ec;
		std::filesystem::remove(path_, ec);
	}

	std::filesystem::path const& path() const { return path_; }

private:
	std::filesystem::path path_;
};

std::string makeRandomSuffix() {
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<int> dist(0, 0xFFFFFF);
	return std::to_string(dist(gen));
}

ScopedTempFile makeTempDatabasePath() {
	auto base = std::filesystem::temp_directory_path();
	return ScopedTempFile(base / ("patch_database_search_" + makeRandomSuffix() + ".db3"));
}

midikraft::PatchHolder makeBankedPatch(std::shared_ptr<DummySynth> synth,
	std::string const& name,
	int bankNo,
	int programNo,
	uint8 dataByte,
	std::shared_ptr<midikraft::SourceInfo> sourceInfo) {
	auto holder = makePatchHolder(synth, name, { dataByte });
	auto bank = MidiBankNumber::fromZeroBase(bankNo, synth->numberOfPatches());
	holder.setBank(bank);
	holder.setPatchNumber(MidiProgramNumber::fromZeroBaseWithBank(bank, programNo));
	holder.setSourceInfo(std::move(sourceInfo));
	return holder;
}

std::vector<std::string> namesFromPatches(std::vector<midikraft::PatchHolder> const& patches) {
	std::vector<std::string> names;
	names.reserve(patches.size());
	for (auto const& patch : patches) {
		names.push_back(patch.name());
	}
	return names;
}

void expectNames(std::vector<midikraft::PatchHolder> const& patches, std::vector<std::string> const& expected) {
	REQUIRE(patches.size() == expected.size());
	for (size_t i = 0; i < expected.size(); ++i) {
		CHECK(patches[i].name() == expected[i]);
	}
}

struct ExpectedPatchOrder {
	std::string name;
	int bankNo;
	int programNo;
};

void expectPatchOrder(std::vector<midikraft::PatchHolder> const& patches, std::vector<ExpectedPatchOrder> const& expected) {
	REQUIRE(patches.size() == expected.size());
	for (size_t i = 0; i < expected.size(); ++i) {
		CHECK(patches[i].name() == expected[i].name);
		CHECK(patches[i].bankNumber().toZeroBased() == expected[i].bankNo);
		CHECK(patches[i].patchNumber().toZeroBasedDiscardingBank() == expected[i].programNo);
	}
}

std::string findListId(std::vector<midikraft::ListInfo> const& lists, std::string const& name) {
	for (auto const& info : lists) {
		if (info.name == name) {
			return info.id;
		}
	}
	return {};
}

} // namespace

TEST_CASE("patch database basic search ordering for single synth") {
	auto tmp = makeTempDatabasePath();
	midikraft::PatchDatabase db(tmp.path().string(), midikraft::PatchDatabase::OpenMode::READ_WRITE);

	auto synth = std::make_shared<DummySynth>("DummySynth", 4, 2);
	auto importSource = std::make_shared<midikraft::FromFileSource>("bulk.syx", "/tmp/bulk.syx", MidiProgramNumber::invalidProgram());

	auto patchOmega = makeBankedPatch(synth, "Omega", 0, 0, 0x01, importSource);
	auto patchAlpha = makeBankedPatch(synth, "Alpha", 1, 0, 0x02, importSource);
	auto patchBeta = makeBankedPatch(synth, "Beta", 1, 0, 0x03, importSource);
	auto patchGamma = makeBankedPatch(synth, "Gamma", 0, 1, 0x04, importSource);
	auto patchZebra = makeBankedPatch(synth, "Zebra", 0, 2, 0x05, importSource);
	auto patchAaron = makeBankedPatch(synth, "Aaron", 0, 3, 0x06, importSource);
	auto patchAlphaTwo = makeBankedPatch(synth, "Alpha", 1, 1, 0x07, importSource);
	auto patchGammaTwo = makeBankedPatch(synth, "Gamma", 1, 3, 0x08, importSource);

	std::vector<midikraft::PatchHolder> patches = {
		patchOmega,
		patchAlphaTwo,
		patchBeta,
		patchAlpha,
		patchGammaTwo,
		patchGamma,
		patchZebra,
		patchAaron,
	};

	for (auto const& patch : patches) {
		db.putPatch(patch);
	}

	auto list = std::make_shared<midikraft::PatchList>("test-list", "Test List");
	list->setPatches({ patchGamma, patchOmega, patchAaron, patchZebra, patchAlpha, patchAlphaTwo, patchBeta, patchGammaTwo });
	db.putPatchList(list);

	std::vector<midikraft::PatchHolder> importOrder = { patchBeta, patchOmega, patchGamma, patchAlpha, patchZebra, patchAaron, patchAlphaTwo, patchGammaTwo };
	db.createImportLists(importOrder);

	auto makeFilter = [&synth]() {
		std::vector<std::shared_ptr<midikraft::Synth>> synths = { synth };
		return midikraft::PatchFilter(synths);
	};

	SUBCASE("no ordering returns all patches") {
		auto filter = makeFilter();
		filter.orderBy = midikraft::PatchOrdering::No_ordering;
		auto result = db.getPatches(filter, 0, -1);

		CHECK(result.size() == patches.size());
		auto names = namesFromPatches(result);
		auto expected = namesFromPatches(patches);
		std::sort(names.begin(), names.end());
		std::sort(expected.begin(), expected.end());
		CHECK(names == expected);
	}

	SUBCASE("order by name") {
		auto filter = makeFilter();
		filter.orderBy = midikraft::PatchOrdering::Order_by_Name;
		auto result = db.getPatches(filter, 0, -1);
		expectPatchOrder(result, {
			{ "Aaron", 0, 3 },
			{ "Alpha", 1, 0 },
			{ "Alpha", 1, 1 },
			{ "Beta", 1, 0 },
			{ "Gamma", 0, 1 },
			{ "Gamma", 1, 3 },
			{ "Omega", 0, 0 },
			{ "Zebra", 0, 2 }
		});
	}

	SUBCASE("order by program number") {
		auto filter = makeFilter();
		filter.orderBy = midikraft::PatchOrdering::Order_by_ProgramNo;
		auto result = db.getPatches(filter, 0, -1);
		expectPatchOrder(result, {
			{ "Omega", 0, 0 },
			{ "Gamma", 0, 1 },
			{ "Zebra", 0, 2 },
			{ "Aaron", 0, 3 },
			{ "Alpha", 1, 0 },
			{ "Beta", 1, 0 },
			{ "Alpha", 1, 1 },
			{ "Gamma", 1, 3 }
		});
	}

	SUBCASE("order by bank number") {
		auto filter = makeFilter();
		filter.orderBy = midikraft::PatchOrdering::Order_by_BankNo;
		auto result = db.getPatches(filter, 0, -1);
		expectPatchOrder(result, {
			{ "Omega", 0, 0 },
			{ "Gamma", 0, 1 },
			{ "Zebra", 0, 2 },
			{ "Aaron", 0, 3 },
			{ "Alpha", 1, 0 },
			{ "Beta", 1, 0 },
			{ "Alpha", 1, 1 },
			{ "Gamma", 1, 3 }
		});
	}

	SUBCASE("order by import list") {
		auto filter = makeFilter();
		filter.orderBy = midikraft::PatchOrdering::Order_by_Import_id;
		auto result = db.getPatches(filter, 0, -1);
		expectPatchOrder(result, {
			{ "Beta", 1, 0 },
			{ "Omega", 0, 0 },
			{ "Gamma", 0, 1 },
			{ "Alpha", 1, 0 },
			{ "Zebra", 0, 2 },
			{ "Aaron", 0, 3 },
			{ "Alpha", 1, 1 },
			{ "Gamma", 1, 3 }
		});
	}

	SUBCASE("order by place in list") {
		auto filter = makeFilter();
		filter.orderBy = midikraft::PatchOrdering::Order_by_Place_in_List;
		filter.listID = list->id();
		auto result = db.getPatches(filter, 0, -1);
		expectPatchOrder(result, {
			{ "Gamma", 0, 1 },
			{ "Omega", 0, 0 },
			{ "Aaron", 0, 3 },
			{ "Zebra", 0, 2 },
			{ "Alpha", 1, 0 },
			{ "Alpha", 1, 1 },
			{ "Beta", 1, 0 },
			{ "Gamma", 1, 3 }
		});
	}
}

TEST_CASE("patch database searches across lists with second synth present") {
	auto tmp = makeTempDatabasePath();
	midikraft::PatchDatabase db(tmp.path().string(), midikraft::PatchDatabase::OpenMode::READ_WRITE);

	auto synthA = std::make_shared<DummySynth>("SearchSynthA", 4, 2);
	auto synthB = std::make_shared<DummySynth>("SearchSynthB", 4, 1);

	auto importSourceA = std::make_shared<midikraft::FromFileSource>("importA.syx", "/tmp/importA.syx", MidiProgramNumber::invalidProgram());
	auto importSourceB = std::make_shared<midikraft::FromFileSource>("importB.syx", "/tmp/importB.syx", MidiProgramNumber::invalidProgram());

	auto patchA1 = makeBankedPatch(synthA, "Normal-1", 0, 0, 0x01, importSourceA);
	auto patchA2 = makeBankedPatch(synthA, "Fav-1", 0, 1, 0x02, importSourceA);
	auto patchA3 = makeBankedPatch(synthA, "Hidden-1", 1, 0, 0x03, importSourceB);
	auto patchA4 = makeBankedPatch(synthA, "Regular-1", 1, 1, 0x04, importSourceB);

	auto patchB1 = makeBankedPatch(synthB, "Normal-2", 0, 0, 0x01, importSourceA);
	auto patchB2 = makeBankedPatch(synthB, "Fav-2", 0, 1, 0x02, importSourceB);

	patchA2.setFavorite(midikraft::Favorite(true));
	patchB2.setFavorite(midikraft::Favorite(true));
	patchA3.setHidden(true);
	patchA4.setRegular(true);

	for (auto const& patch : { patchA1, patchA2, patchA3, patchA4, patchB1, patchB2 }) {
		db.putPatch(patch);
	}

	auto userList1 = std::make_shared<midikraft::PatchList>("user-list-1", "User List 1");
	userList1->setPatches({ patchA2, patchA4 });
	db.putPatchList(userList1);

	auto userList2 = std::make_shared<midikraft::PatchList>("user-list-2", "User List 2");
	userList2->setPatches({ patchA3, patchA1 });
	db.putPatchList(userList2);

	auto mixedUserList = std::make_shared<midikraft::PatchList>("user-list-mixed", "User List Mixed");
	mixedUserList->setPatches({ patchA1, patchB2, patchA4 });
	db.putPatchList(mixedUserList);

	std::vector<midikraft::PatchHolder> importOrder = { patchA2, patchA1, patchA4, patchA3 };
	db.createImportLists(importOrder);

	auto importLists = db.allImportLists(synthA);
	REQUIRE(importLists.size() >= 2);
	auto importListAId = findListId(importLists, "Imported from file importA.syx");
	auto importListBId = findListId(importLists, "Imported from file importB.syx");
	REQUIRE(!importListAId.empty());
	REQUIRE(!importListBId.empty());

	auto makeFilter = [&synthA]() {
		std::vector<std::shared_ptr<midikraft::Synth>> synths = { synthA };
		return midikraft::PatchFilter(synths);
	};
	auto makeFilterAll = [&synthA, &synthB]() {
		std::vector<std::shared_ptr<midikraft::Synth>> synths = { synthA, synthB };
		return midikraft::PatchFilter(synths);
	};

	SUBCASE("all patches from synth A ignore synth B") {
		auto filter = makeFilter();
		filter.orderBy = midikraft::PatchOrdering::Order_by_Name;
		auto result = db.getPatches(filter, 0, -1);
		expectNames(result, { "Fav-1", "Normal-1", "Regular-1" });
	}

	SUBCASE("user list search uses list order") {
		auto filter = makeFilter();
		filter.orderBy = midikraft::PatchOrdering::Order_by_Place_in_List;
		filter.listID = userList1->id();
		auto result = db.getPatches(filter, 0, -1);
		expectNames(result, { "Fav-1", "Regular-1" });
	}

	SUBCASE("import list search uses list order") {
		auto filter = makeFilter();
		filter.orderBy = midikraft::PatchOrdering::Order_by_Place_in_List;
		filter.listID = importListBId;
		auto result = db.getPatches(filter, 0, -1);
		expectNames(result, { "Regular-1" });
	}

	SUBCASE("all patches across both synths") {
		auto filter = makeFilterAll();
		filter.orderBy = midikraft::PatchOrdering::Order_by_Name;
		auto result = db.getPatches(filter, 0, -1);
		expectNames(result, { "Fav-1", "Fav-2", "Normal-1", "Normal-2", "Regular-1" });
	}

	SUBCASE("user list can contain patches from multiple synths") {
		auto filter = makeFilterAll();
		filter.orderBy = midikraft::PatchOrdering::Order_by_Place_in_List;
		filter.listID = mixedUserList->id();
		auto result = db.getPatches(filter, 0, -1);
		expectNames(result, { "Normal-1", "Fav-2", "Regular-1" });
	}

	struct ListCase {
		const char* label;
		std::function<void(midikraft::PatchFilter&)> applyList;
		midikraft::PatchOrdering ordering;
		std::vector<std::reference_wrapper<midikraft::PatchHolder>> base;
	};

	struct VisibilityCase {
		std::string label;
		bool onlyFaves = false;
		bool showHidden = false;
		bool showRegular = false;
		bool showUndecided = false;
	};

	std::vector<ListCase> listCases = {
		{
			"all patches across both synths",
			[](midikraft::PatchFilter&) {},
			midikraft::PatchOrdering::Order_by_Name,
			{ patchA2, patchB2, patchA3, patchA1, patchB1, patchA4 }
		},
		{
			"user list 1",
			[&userList1](midikraft::PatchFilter& filter) { filter.listID = userList1->id(); },
			midikraft::PatchOrdering::Order_by_Place_in_List,
			{ patchA2, patchA4 }
		},
		{
			"user list mixed",
			[&mixedUserList](midikraft::PatchFilter& filter) { filter.listID = mixedUserList->id(); },
			midikraft::PatchOrdering::Order_by_Place_in_List,
			{ patchA1, patchB2, patchA4 }
		},
		{
			"import list B",
			[&importListBId](midikraft::PatchFilter& filter) { filter.listID = importListBId; },
			midikraft::PatchOrdering::Order_by_Place_in_List,
			{ patchA4, patchA3 }
		},
	};

	auto matchesVisibility = [](VisibilityCase const& visibility, midikraft::PatchHolder const& patch) {
		bool hasAnyFilter = visibility.onlyFaves || visibility.showHidden || visibility.showRegular || visibility.showUndecided;
		if (!hasAnyFilter) {
			return !patch.isHidden();
		}
		bool undecided = !patch.isFavorite() && !patch.isHidden() && !patch.isRegular();
		bool positive = false;
		if (visibility.onlyFaves) positive = positive || patch.isFavorite();
		if (visibility.showHidden) positive = positive || patch.isHidden();
		if (visibility.showRegular) positive = positive || patch.isRegular();
		if (visibility.showUndecided) positive = positive || undecided;
		bool negative = true;
		if (!visibility.onlyFaves) negative = negative && !patch.isFavorite();
		if (!visibility.showHidden) negative = negative && !patch.isHidden();
		if (!visibility.showRegular) negative = negative && !patch.isRegular();
		return positive && negative;
	};

	auto expectedNamesFor = [&](ListCase const& listCase, VisibilityCase const& visibility) {
		std::vector<std::string> expected;
		for (auto const& patchRef : listCase.base) {
			auto const& patch = patchRef.get();
			if (matchesVisibility(visibility, patch)) {
				expected.push_back(patch.name());
			}
		}
		return expected;
	};

	auto orderingKey = [](midikraft::PatchHolder const& patch, midikraft::PatchOrdering ordering) {
		switch (ordering) {
		case midikraft::PatchOrdering::Order_by_Name:
			return patch.name() + "|" + std::to_string(patch.bankNumber().toZeroBased()) + "|" + std::to_string(patch.patchNumber().toZeroBasedDiscardingBank());
		case midikraft::PatchOrdering::Order_by_BankNo:
			return std::to_string(patch.bankNumber().toZeroBased()) + "|" + std::to_string(patch.patchNumber().toZeroBasedDiscardingBank()) + "|" + patch.name();
		case midikraft::PatchOrdering::Order_by_ProgramNo:
			return std::to_string(patch.patchNumber().toZeroBasedDiscardingBank()) + "|" + patch.name();
		default:
			return patch.name();
		}
	};

	auto assertBaseOrderMatches = [&](ListCase const& listCase) {
		if (listCase.ordering == midikraft::PatchOrdering::Order_by_Place_in_List) {
			return;
		}
		std::string lastKey;
		for (auto const& patchRef : listCase.base) {
			auto const& patch = patchRef.get();
			auto key = orderingKey(patch, listCase.ordering);
			if (!lastKey.empty()) {
				CHECK(lastKey <= key);
			}
			lastKey = std::move(key);
		}
	};

	std::vector<VisibilityCase> visibilityCases;
	for (int mask = 0; mask < 16; ++mask) {
		VisibilityCase visibility;
		visibility.onlyFaves = (mask & 0x1) != 0;
		visibility.showHidden = (mask & 0x2) != 0;
		visibility.showRegular = (mask & 0x4) != 0;
		visibility.showUndecided = (mask & 0x8) != 0;
		visibility.label = "faves=" + std::to_string(visibility.onlyFaves)
			+ " hidden=" + std::to_string(visibility.showHidden)
			+ " regular=" + std::to_string(visibility.showRegular)
			+ " undecided=" + std::to_string(visibility.showUndecided);
		visibilityCases.push_back(visibility);
	}

	for (auto const& listCase : listCases) {
		assertBaseOrderMatches(listCase);
		for (auto const& visibility : visibilityCases) {
			auto label = std::string(listCase.label) + " + " + visibility.label;
			SUBCASE(label.c_str()) {
				auto filter = makeFilterAll();
				listCase.applyList(filter);
				filter.onlyFaves = visibility.onlyFaves;
				filter.showHidden = visibility.showHidden;
				filter.showRegular = visibility.showRegular;
				filter.showUndecided = visibility.showUndecided;
				filter.orderBy = listCase.ordering;
				auto result = db.getPatches(filter, 0, -1);
				auto expected = expectedNamesFor(listCase, visibility);
				expectNames(result, expected);
			}
		}
	}
}
