//
// This file is part of the aMule Project.
//
// Copyright (c) 2003-2026 aMule Team ( https://amule-org.github.io )
//
// Any parts of this program derived from the xMule, lMule or eMule project,
// or contributed by third-party developers are copyrighted by their
// respective authors.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301, USA
//

#include <muleunit/test.h>
#include "SearchPacketWriter.h"
#include <string>
#include <map>

using namespace muleunit;
DECLARE_SIMPLE(SearchMetadataPacket)

namespace
{
std::string Packet(const CSearchList::CSearchParams &params,
	const wxArrayString &expr,
	bool supports64bit = true,
	bool *large = nullptr)
{
	CMemFile data;
	bool using64bit;
	CSearchExprTarget target(&data, utf8strRaw, supports64bit, using64bit);
	WriteSearchPacket(params, expr, target);
	if (large) {
		*large = using64bit;
	}
	return data.GetLength() == 0
		       ? std::string()
		       : std::string(reinterpret_cast<const char *>(data.GetRawBuffer()), data.GetLength());
}

std::string Hex(const std::string &bytes)
{
	static const char digits[] = "0123456789abcdef";
	std::string result;
	for (unsigned char byte : bytes) {
		result += digits[byte >> 4];
		result += digits[byte & 15];
	}
	return result;
}

struct MediaFile
{
	wxString name;
	std::map<uint8, wxString> text;
	std::map<uint8, uint64> numbers;
};

bool Evaluate(CMemFile &packet, const MediaFile &file)
{
	const auto kind = packet.ReadUInt8();
	if (kind == 0) {
		const auto op = packet.ReadUInt8();
		const bool left = Evaluate(packet, file);
		const bool right = Evaluate(packet, file);
		ASSERT_TRUE(op <= 2);
		return op == 0 ? left && right : op == 1 ? left || right : left && !right;
	}
	if (kind == 1) {
		return file.name.Find(packet.ReadString(true)) != wxNOT_FOUND;
	}
	if (kind == 2) {
		const auto value = packet.ReadString(true);
		ASSERT_EQUALS(1, packet.ReadUInt16());
		const auto tag = packet.ReadUInt8();
		const auto found = file.text.find(tag);
		return found != file.text.end() && found->second.CmpNoCase(value) == 0;
	}
	ASSERT_TRUE(kind == 3 || kind == 8);
	const uint64 value = kind == 3 ? packet.ReadUInt32() : packet.ReadUInt64();
	const auto op = packet.ReadUInt8();
	ASSERT_EQUALS(1, packet.ReadUInt16());
	const auto tag = packet.ReadUInt8();
	const auto found = file.numbers.find(tag);
	if (found == file.numbers.end()) {
		return false;
	}
	ASSERT_EQUALS(ED2K_SEARCH_OP_GREATER_EQUAL, op);
	return found->second >= value;
}

bool Matches(const std::string &bytes, const MediaFile &file)
{
	CMemFile packet(reinterpret_cast<const uint8 *>(bytes.data()), bytes.size());
	const bool result = Evaluate(packet, file);
	ASSERT_EQUALS(packet.GetLength(), packet.GetPosition());
	return result;
}

// Decode a complete prefix expression independently of the production composer.
void ReadExpression(CMemFile &packet, unsigned &leaves)
{
	const auto kind = packet.ReadUInt8();
	if (kind == 0) {
		ASSERT_TRUE(packet.ReadUInt8() <= 2);
		ReadExpression(packet, leaves);
		ReadExpression(packet, leaves);
		return;
	}
	++leaves;
	if (kind == 1 || kind == 2) {
		packet.ReadString(true);
	} else {
		ASSERT_TRUE(kind == 3 || kind == 8);
		if (kind == 3) {
			packet.ReadUInt32();
		} else {
			packet.ReadUInt64();
		}
		packet.ReadUInt8();
	}
	if (kind != 1) {
		ASSERT_EQUALS(1, packet.ReadUInt16());
		packet.ReadUInt8();
	}
}
} // namespace

TEST(SearchMetadataPacket, LegacyPacketsKeepExactBytes)
{
	CSearchList::CSearchParams params;
	wxArrayString expr;
	expr.Add("cat");
	ASSERT_EQUALS(std::string("010300636174"), Hex(Packet(params, expr)));
	params.minSize = 1;
	params.maxSize = 2;
	params.availability = 3;
	params.extension = "mp3";
	const std::string filters =
		"0301000000010100020302000000020100020303000000010100150203006d7033010004";
	ASSERT_EQUALS(std::string("00000103006361740000030100000001010002000003020000000201000200000303000000"
				  "010100150203006d7033010004"),
		Hex(Packet(params, expr)));
	expr.Clear();
	expr.Add(SEARCHOPTOK_OR);
	expr.Add("cat");
	expr.Add("dog");
	ASSERT_EQUALS(std::string("00000000000000000001010300636174010300646f67") + filters,
		Hex(Packet(params, expr)));
}

TEST(SearchMetadataPacket, MetadataUsesNumericTagsAndInclusiveMinimums)
{
	CSearchList::CSearchParams params;
	wxArrayString expr;
	params.minBitrateKbps = 128;
	params.minLength = 60;
	params.codec = "mp3";
	params.title = "song";
	params.album = "record";
	params.artist = "artist";
	ASSERT_EQUALS(std::string("00000380000000030100d40000033c000000030100d300000203006d70330100d500000204"
				  "00736f6e670100d200000206007265636f72640100d10206006172746973740100d0"),
		Hex(Packet(params, expr)));
}

TEST(SearchMetadataPacket, CombinedMetadataAndBooleanExpressionsAreCompleteTrees)
{
	for (const auto &op : { SEARCHOPTOK_OR, SEARCHOPTOK_NOT }) {
		CSearchList::CSearchParams params;
		params.minBitrateKbps = 192;
		params.minLength = 120;
		params.title = wxString::FromUTF8("歌曲");
		wxArrayString expr;
		expr.Add(op);
		expr.Add("cat");
		expr.Add("dog");
		const auto bytes = Packet(params, expr);
		CMemFile packet(reinterpret_cast<const uint8 *>(bytes.data()), bytes.size());
		unsigned leaves = 0;
		ReadExpression(packet, leaves);
		ASSERT_EQUALS(5u, leaves);
		ASSERT_EQUALS(packet.GetLength(), packet.GetPosition());
	}
}

TEST(SearchMetadataPacket, LargeSizeEncodingAndClampingRemainIntact)
{
	CSearchList::CSearchParams params;
	params.minSize = UINT64_C(1) << 32;
	wxArrayString expr;
	bool large = false;
	ASSERT_EQUALS(std::string("08000000000100000001010002"), Hex(Packet(params, expr, true, &large)));
	ASSERT_TRUE(large);
	ASSERT_EQUALS(std::string("03ffffffff01010002"), Hex(Packet(params, expr, false, &large)));
	ASSERT_TRUE(!large);
}

TEST(SearchMetadataPacket, EveryFilterSubsetFormsACompleteExpression)
{
	for (unsigned mask = 0; mask < 64; ++mask) {
		for (unsigned kind = 0; kind < 3; ++kind) {
			CSearchList::CSearchParams params;
			params.minBitrateKbps = (mask & 1) ? 128 : 0;
			params.minLength = (mask & 2) ? 60 : 0;
			params.codec = (mask & 4) ? "mp3" : "";
			params.title = (mask & 8) ? "song" : "";
			params.album = (mask & 16) ? "album" : "";
			params.artist = (mask & 32) ? "artist" : "";
			wxArrayString expr;
			if (kind == 2) {
				expr.Add(SEARCHOPTOK_OR);
			}
			if (kind > 0) {
				expr.Add("cat");
			}
			if (kind == 2) {
				expr.Add("dog");
			}
			const auto bytes = Packet(params, expr);
			if (bytes.empty()) {
				ASSERT_EQUALS(0u, mask);
				ASSERT_EQUALS(0u, kind);
				continue;
			}
			CMemFile packet(reinterpret_cast<const uint8 *>(bytes.data()), bytes.size());
			unsigned leaves = 0;
			ReadExpression(packet, leaves);
			unsigned expected = kind;
			for (unsigned bits = mask; bits; bits >>= 1) {
				expected += bits & 1;
			}
			ASSERT_EQUALS(expected, leaves);
			ASSERT_EQUALS(packet.GetLength(), packet.GetPosition());
		}
	}
}

TEST(SearchMetadataPacket, FileTypeMappingAndKadKeywordOnlyPacketsStayCompatible)
{
	CSearchList::CSearchParams params;
	wxArrayString expr;
	// Kad's selected keyword may be removed by the parser, leaving just filters.
	ASSERT_EQUALS(std::string(), Packet(params, expr));
	params.typeText = ED2KFTSTR_ARCHIVE;
	ASSERT_EQUALS(std::string("02030050726f010003"), Hex(Packet(params, expr)));
	params.typeText = ED2KFTSTR_CDIMAGE;
	ASSERT_EQUALS(std::string("02030050726f010003"), Hex(Packet(params, expr)));
	// The historical complex-expression branch sends the original type string.
	expr.Add(SEARCHOPTOK_NOT);
	expr.Add("cat");
	expr.Add("dog");
	params.typeText = ED2KFTSTR_ARCHIVE;
	ASSERT_EQUALS(
		std::string("00000002010300636174010300646f67020300417263010003"), Hex(Packet(params, expr)));
}

TEST(SearchMetadataPacket, SingleLiteralOperatorMarkersRemainStrings)
{
	// The lexer accepts these Unicode strings as literals. Only multi-token
	// prefix expressions interpret the parser's magic strings as operators.
	for (const auto &literal : { SEARCHOPTOK_AND, SEARCHOPTOK_OR, SEARCHOPTOK_NOT }) {
		CSearchList::CSearchParams params;
		wxArrayString expr;
		expr.Add(literal);
		const auto bytes = Packet(params, expr);
		CMemFile packet(reinterpret_cast<const uint8 *>(bytes.data()), bytes.size());
		ASSERT_EQUALS(1, packet.ReadUInt8());
		ASSERT_EQUALS(wxString(literal), packet.ReadString(true));
		ASSERT_EQUALS(packet.GetLength(), packet.GetPosition());
	}
}

TEST(SearchMetadataPacket, MetadataConstrainsBothBooleanBranches)
{
	for (const auto &op : { SEARCHOPTOK_OR, SEARCHOPTOK_NOT }) {
		CSearchList::CSearchParams params;
		params.minBitrateKbps = 128;
		params.minLength = 60;
		params.codec = "mp3";
		params.title = wxString::FromUTF8("歌曲");
		params.album = "album";
		params.artist = "artist";
		wxArrayString expr;
		expr.Add(op);
		expr.Add("cat");
		expr.Add("dog");
		const auto bytes = Packet(params, expr);
		MediaFile file;
		file.name = "cat";
		file.numbers = { { FT_MEDIA_BITRATE, 128 }, { FT_MEDIA_LENGTH, 60 } };
		file.text = { { FT_MEDIA_CODEC, "MP3" },
			{ FT_MEDIA_TITLE, params.title },
			{ FT_MEDIA_ALBUM, "album" },
			{ FT_MEDIA_ARTIST, "artist" } };
		ASSERT_TRUE(Matches(bytes, file));
		file.name = "dog";
		ASSERT_EQUALS(op == wxString(SEARCHOPTOK_OR), Matches(bytes, file));
		file.name = "cat dog";
		ASSERT_EQUALS(op == wxString(SEARCHOPTOK_OR), Matches(bytes, file));
		file.name = "bird";
		ASSERT_TRUE(!Matches(bytes, file));
		for (const auto &name : { "cat", "dog" }) {
			file.name = name;
			for (const auto &tag : { FT_MEDIA_BITRATE, FT_MEDIA_LENGTH }) {
				auto mismatch = file;
				--mismatch.numbers[tag];
				ASSERT_TRUE(!Matches(bytes, mismatch));
				mismatch.numbers.erase(tag);
				ASSERT_TRUE(!Matches(bytes, mismatch));
			}
			for (const auto &tag :
				{ FT_MEDIA_CODEC, FT_MEDIA_TITLE, FT_MEDIA_ALBUM, FT_MEDIA_ARTIST }) {
				auto mismatch = file;
				mismatch.text[tag] = "wrong";
				ASSERT_TRUE(!Matches(bytes, mismatch));
				mismatch.text.erase(tag);
				ASSERT_TRUE(!Matches(bytes, mismatch));
			}
		}
	}
}
