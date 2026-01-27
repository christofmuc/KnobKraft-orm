#include "doctest/doctest.h"

#include "PatchDatabase.h"
#include "PatchList.h"
#include "test_helpers.h"

#include <algorithm>
#include <filesystem>
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

	std::vector<midikraft::PatchHolder> patches = {
		patchOmega,
		patchAlpha,
		patchBeta,
		patchGamma,
		patchZebra
	};

	for (auto const& patch : patches) {
		db.putPatch(patch);
	}

	auto list = std::make_shared<midikraft::PatchList>("test-list", "Test List");
	list->setPatches({ patchGamma, patchOmega, patchZebra, patchAlpha, patchBeta });
	db.putPatchList(list);

	std::vector<midikraft::PatchHolder> importOrder = { patchBeta, patchOmega, patchGamma, patchAlpha, patchZebra };
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
		expectNames(result, { "Alpha", "Beta", "Gamma", "Omega", "Zebra" });
	}

	SUBCASE("order by program number") {
		auto filter = makeFilter();
		filter.orderBy = midikraft::PatchOrdering::Order_by_ProgramNo;
		auto result = db.getPatches(filter, 0, -1);
		expectNames(result, { "Omega", "Gamma", "Zebra", "Alpha", "Beta" });
	}

	SUBCASE("order by bank number") {
		auto filter = makeFilter();
		filter.orderBy = midikraft::PatchOrdering::Order_by_BankNo;
		auto result = db.getPatches(filter, 0, -1);
		expectNames(result, { "Omega", "Gamma", "Zebra", "Alpha", "Beta" });
	}

	SUBCASE("order by import list") {
		auto filter = makeFilter();
		filter.orderBy = midikraft::PatchOrdering::Order_by_Import_id;
		auto result = db.getPatches(filter, 0, -1);
		expectNames(result, { "Beta", "Omega", "Gamma", "Alpha", "Zebra" });
	}

	SUBCASE("order by place in list") {
		auto filter = makeFilter();
		filter.orderBy = midikraft::PatchOrdering::Order_by_Place_in_List;
		filter.listID = list->id();
		auto result = db.getPatches(filter, 0, -1);
		expectNames(result, { "Gamma", "Omega", "Zebra", "Alpha", "Beta" });
	}
}
