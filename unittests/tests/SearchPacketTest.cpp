// This file is part of the aMule Project.
// Copyright (c) 2003-2026 aMule Team ( https://amule-org.github.io )
// Licensed under the GNU General Public License, version 2 or later.

#include <muleunit/test.h>
#include "SearchPacket.h"
#include "MemFile.h"
#include <initializer_list>
#include <limits>
#include <string>
#include <utility>
#include <vector>

using namespace muleunit;
DECLARE_SIMPLE(SearchPacket)

using Bytes = std::vector<uint8_t>;

class CSearchPacketTestFixture
{
public:
	static Bytes Packet(CSearchList::CSearchParams params,
		SearchType type,
		bool supports64bit = true,
		bool expected64bit = false)
	{
		bool using64bit = true;
		auto packet = CreateSearchData(params, type, supports64bit, using64bit);
		ASSERT_TRUE(packet.get() != nullptr);
		ASSERT_EQUALS(expected64bit, using64bit);
		return Bytes(packet->GetRawBuffer(), packet->GetRawBuffer() + packet->GetLength());
	}

	static bool Rejected(CSearchList::CSearchParams params, SearchType type)
	{
		bool using64bit = false;
		return !CreateSearchData(params, type, true, using64bit);
	}
};

namespace
{
Bytes Join(std::initializer_list<Bytes> terms)
{
	Bytes result;
	for (const auto &term : terms) {
		result.insert(result.end(), term.begin(), term.end());
	}
	return result;
}

// Expected bytes are constructed independently of CMemFile and the production writer.
Bytes Text(const char *value)
{
	const std::string text(value);
	Bytes result{ 1, static_cast<uint8_t>(text.size()), 0 };
	result.insert(result.end(), text.begin(), text.end());
	return result;
}

Bytes Number(uint32_t value, uint8_t comparison, uint8_t tag)
{
	return { 3,
		static_cast<uint8_t>(value),
		static_cast<uint8_t>(value >> 8),
		static_cast<uint8_t>(value >> 16),
		static_cast<uint8_t>(value >> 24),
		comparison,
		1,
		0,
		tag };
}

Bytes Extension()
{
	return { 2, 3, 0, 'm', 'p', '3', 1, 0, 0x04 };
}

const Bytes And{ 0, 0 };
const Bytes Or{ 0, 1 };
const Bytes Not{ 0, 2 };
// eD2k/Kad numeric attribute ID 0xD4, operator 0x03 (>=).
Bytes Bitrate(uint32_t value)
{
	return Number(value, 3, 0xD4);
}
} // namespace

TEST(SearchPacket, DefaultBitrateKeepsExistingPackets)
{
	CSearchList::CSearchParams params;
	ASSERT_EQUALS(uint32_t(0), params.minBitrateKbps);
	params.searchString = "song";
	params.strKeyword = "song";
	for (const auto type : { LocalSearch, GlobalSearch, KadSearch }) {
		const Bytes query = type == KadSearch ? Bytes{} : Text("song");
		ASSERT_TRUE(CSearchPacketTestFixture::Packet(params, type) == query);
		params.minSize = 100;
		params.extension = "mp3";
		const Bytes expected = type == KadSearch
					       ? Join({ And, Number(100, 1, 0x02), Extension() })
					       : Join({ And, query, And, Number(100, 1, 0x02), Extension() });
		ASSERT_TRUE(CSearchPacketTestFixture::Packet(params, type) == expected);
		params.minSize = 0;
		params.extension.Clear();
	}
}

TEST(SearchPacket, SimpleQueryUsesInclusiveNumericBitrateOnBothNetworks)
{
	CSearchList::CSearchParams params;
	params.searchString = "song";
	params.strKeyword = "song";
	for (const auto bitrate : { 1u, 320u, std::numeric_limits<uint32_t>::max() }) {
		params.minBitrateKbps = bitrate;
		for (const bool supports64bit : { false, true }) {
			for (const auto type : { LocalSearch, GlobalSearch, KadSearch }) {
				const Bytes expected =
					type == KadSearch ? Bitrate(bitrate)
							  : Join({ And, Text("song"), Bitrate(bitrate) });
				ASSERT_TRUE(CSearchPacketTestFixture::Packet(params, type, supports64bit) ==
					    expected);
			}
		}
	}
}

TEST(SearchPacket, SimpleQueryCountsBitrateAlongsideExistingFilters)
{
	CSearchList::CSearchParams params;
	params.searchString = "song";
	params.strKeyword = "song";
	params.minSize = 100;
	params.extension = "mp3";
	params.minBitrateKbps = 320;
	for (const auto type : { GlobalSearch, KadSearch }) {
		const Bytes filters = Join({ And, Number(100, 1, 0x02), And, Extension(), Bitrate(320) });
		const Bytes expected = type == KadSearch ? filters : Join({ And, Text("song"), filters });
		ASSERT_TRUE(CSearchPacketTestFixture::Packet(params, type) == expected);
	}
}

TEST(SearchPacket, BooleanQueriesKeepTheirTreeAndAndTheBitrateFilter)
{
	for (const auto &op : { std::make_pair(" OR ", Or), std::make_pair(" NOT ", Not) }) {
		CSearchList::CSearchParams params;
		params.searchString = "song" + wxString(op.first) + "live";
		params.strKeyword = "song";
		const Bytes query = Join({ op.second, Text("song"), Text("live") });
		for (const auto type : { GlobalSearch, KadSearch }) {
			ASSERT_TRUE(CSearchPacketTestFixture::Packet(params, type) == query);
			params.minBitrateKbps = 320;
			ASSERT_TRUE(CSearchPacketTestFixture::Packet(params, type) ==
				    Join({ And, query, Bitrate(320) }));
			params.minSize = 100;
			params.extension = "mp3";
			ASSERT_TRUE(CSearchPacketTestFixture::Packet(params, type) ==
				    Join({ And,
					    And,
					    And,
					    query,
					    Number(100, 1, 0x02),
					    Extension(),
					    Bitrate(320) }));
			params.minBitrateKbps = 0;
			params.minSize = 0;
			params.extension.Clear();
		}
	}
}

TEST(SearchPacket, NestedOrNotQueryKeepsItsGrouping)
{
	CSearchList::CSearchParams params;
	params.searchString = "(song OR live) NOT remix";
	params.strKeyword = "song";
	const Bytes query = Join({ Not, Or, Text("song"), Text("live"), Text("remix") });
	for (const auto type : { GlobalSearch, KadSearch }) {
		ASSERT_TRUE(CSearchPacketTestFixture::Packet(params, type) == query);
		params.minBitrateKbps = 320;
		ASSERT_TRUE(
			CSearchPacketTestFixture::Packet(params, type) == Join({ And, query, Bitrate(320) }));
		params.minBitrateKbps = 0;
	}
}

TEST(SearchPacket, AllExistingFiltersKeepTheirOrderingAndComparisons)
{
	for (const bool booleanQuery : { false, true }) {
		CSearchList::CSearchParams params;
		params.searchString = booleanQuery ? "song OR live" : "song";
		params.strKeyword = "song";
		params.typeText = "Audio";
		params.minSize = 100;
		params.maxSize = 1000;
		params.availability = 3;
		params.extension = "mp3";
		const Bytes fileType{ 2, 5, 0, 'A', 'u', 'd', 'i', 'o', 1, 0, 0x03 };
		for (const auto type : { LocalSearch, GlobalSearch, KadSearch }) {
			const Bytes query = booleanQuery        ? Join({ Or, Text("song"), Text("live") })
					    : type == KadSearch ? Bytes{}
								: Text("song");
			for (const auto bitrate : { 0u, 320u }) {
				params.minBitrateKbps = bitrate;
				Bytes expected;
				if (booleanQuery) {
					expected = Join({ And,
						And,
						And,
						And,
						And,
						query,
						fileType,
						Number(100, 1, 0x02),
						Number(1000, 2, 0x02),
						Number(3, 1, 0x15),
						Extension() });
					if (bitrate) {
						expected = Join({ And, expected, Bitrate(bitrate) });
					}
				} else {
					expected = Join({ And,
						fileType,
						And,
						Number(100, 1, 0x02),
						And,
						Number(1000, 2, 0x02),
						And,
						Number(3, 1, 0x15),
						bitrate ? Join({ And, Extension(), Bitrate(bitrate) })
							: Extension() });
					if (!query.empty()) {
						expected = Join({ And, query, expected });
					}
				}
				ASSERT_TRUE(CSearchPacketTestFixture::Packet(params, type) == expected);
			}
		}
	}
}

TEST(SearchPacket, KadKeywordRearrangementAndNetworkSwitchKeepBitrate)
{
	CSearchList::CSearchParams params;
	params.searchString = "song instrumental";
	params.strKeyword = "song";
	params.minBitrateKbps = 320;
	// Kad selects the longer routing keyword and sends only the remaining word.
	ASSERT_TRUE(CSearchPacketTestFixture::Packet(params, KadSearch) ==
		    Join({ And, Text("song"), Bitrate(320) }));
	// A later server serialization must restore all filename search words.
	ASSERT_TRUE(CSearchPacketTestFixture::Packet(params, GlobalSearch) ==
		    Join({ And, Text("song instrumental"), Bitrate(320) }));
}

TEST(SearchPacket, LargeSizeStillControls64BitEncodingWhenBitrateIsPresent)
{
	CSearchList::CSearchParams params;
	params.searchString = "song";
	params.strKeyword = "song";
	params.minSize = UINT64_C(1) << 32;
	params.minBitrateKbps = 320;
	const Bytes largeSize{ 8, 0, 0, 0, 0, 1, 0, 0, 0, 1, 1, 0, 0x02 };
	for (const auto type : { GlobalSearch, KadSearch }) {
		for (const bool supports64bit : { false, true }) {
			const Bytes size = supports64bit ? largeSize : Number(UINT32_MAX, 1, 0x02);
			const Bytes filters = Join({ And, size, Bitrate(320) });
			const Bytes expected =
				type == KadSearch ? filters : Join({ And, Text("song"), filters });
			ASSERT_TRUE(CSearchPacketTestFixture::Packet(
					    params, type, supports64bit, supports64bit) == expected);
		}
	}
}

TEST(SearchPacket, InvalidBooleanInputStillFailsWithBitrate)
{
	CSearchList::CSearchParams params;
	params.searchString = "song OR";
	params.strKeyword = "song";
	for (const auto type : { GlobalSearch, KadSearch }) {
		for (const auto bitrate : { 0u, 320u }) {
			params.minBitrateKbps = bitrate;
			ASSERT_TRUE(CSearchPacketTestFixture::Rejected(params, type));
		}
	}
}

TEST(SearchPacket, RejectedQueryDoesNotLeakParserStateIntoNextSearch)
{
	CSearchList::CSearchParams params;
	params.searchString = "song OR";
	params.strKeyword = "song";
	params.minBitrateKbps = 320;
	ASSERT_TRUE(CSearchPacketTestFixture::Rejected(params, KadSearch));
	ASSERT_TRUE(!_astrParserErrors.IsEmpty());
	params.searchString = "live";
	params.minBitrateKbps = 0;
	ASSERT_TRUE(CSearchPacketTestFixture::Packet(params, GlobalSearch) == Text("live"));
	ASSERT_TRUE(_astrParserErrors.IsEmpty());
	params.minBitrateKbps = 256;
	params.strKeyword = "live";
	ASSERT_TRUE(CSearchPacketTestFixture::Packet(params, KadSearch) == Bitrate(256));
	ASSERT_TRUE(_astrParserErrors.IsEmpty());
}
