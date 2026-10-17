//
// This file is part of the aMule Project.
//
// Copyright (c) 2003-2026 aMule Team ( https://amule-org.github.io )
// Copyright (c) 2002-2011 Merkur ( devs@emule-project.net / http://www.emule-project.net )
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

#ifndef SEARCHPACKETWRITER_H
#define SEARCHPACKETWRITER_H

#include "MemFile.h"
#include "SearchList.h"
#include "SearchExpr.h"
#include <protocol/ed2k/Constants.h>
#include <tags/FileTags.h>
#include <functional>
#include <vector>

//! Helper class for packet creation
class CSearchExprTarget
{
public:
	CSearchExprTarget(CMemFile *pData, EUtf8Str eStrEncode, bool supports64bit, bool &using64bit)
	: m_data(pData)
	, m_eStrEncode(eStrEncode)
	, m_supports64bit(supports64bit)
	, m_using64bit(using64bit)
	{
		m_using64bit = false;
	}

	void WriteBooleanAND()
	{
		m_data->WriteUInt8(0);    // boolean operator parameter type
		m_data->WriteUInt8(0x00); // "AND"
	}

	void WriteBooleanOR()
	{
		m_data->WriteUInt8(0);    // boolean operator parameter type
		m_data->WriteUInt8(0x01); // "OR"
	}

	void WriteBooleanNOT()
	{
		m_data->WriteUInt8(0);    // boolean operator parameter type
		m_data->WriteUInt8(0x02); // "NOT"
	}

	void WriteMetaDataSearchParam(const wxString &rstrValue)
	{
		m_data->WriteUInt8(1);                        // string parameter type
		m_data->WriteString(rstrValue, m_eStrEncode); // string value
	}

	void WriteMetaDataSearchParam(uint8 uMetaTagID, const wxString &rstrValue)
	{
		m_data->WriteUInt8(2);                        // string parameter type
		m_data->WriteString(rstrValue, m_eStrEncode); // string value
		m_data->WriteUInt16(sizeof(uint8));           // meta tag ID length
		m_data->WriteUInt8(uMetaTagID);               // meta tag ID name
	}

	void WriteMetaDataSearchParamASCII(uint8 uMetaTagID, const wxString &rstrValue)
	{
		m_data->WriteUInt8(2);                       // string parameter type
		m_data->WriteString(rstrValue, utf8strNone); // string value
		m_data->WriteUInt16(sizeof(uint8));          // meta tag ID length
		m_data->WriteUInt8(uMetaTagID);              // meta tag ID name
	}

	void WriteMetaDataSearchParam(uint8_t uMetaTagID, uint8_t uOperator, uint64_t value)
	{
		bool largeValue = value > wxULL(0xFFFFFFFF);
		if (largeValue && m_supports64bit) {
			m_using64bit = true;
			m_data->WriteUInt8(8);      // numeric parameter type (int64)
			m_data->WriteUInt64(value); // numeric value
		} else {
			if (largeValue) {
				value = 0xFFFFFFFFu;
			}
			m_data->WriteUInt8(3);      // numeric parameter type (int32)
			m_data->WriteUInt32(value); // numeric value
		}
		m_data->WriteUInt8(uOperator);      // comparison operator
		m_data->WriteUInt16(sizeof(uint8)); // meta tag ID length
		m_data->WriteUInt8(uMetaTagID);     // meta tag ID name
	}

protected:
	CMemFile *m_data;
	EUtf8Str m_eStrEncode;
	bool m_supports64bit;
	bool &m_using64bit;
};

// Each complete term is added once. Composition never needs a separate parameter count.
inline void WriteSearchPacket(
	const CSearchList::CSearchParams &params, const wxArrayString &expression, CSearchExprTarget &target)
{
	std::vector<std::function<void()>> terms;
	if (expression.GetCount() == 1) {
		// A single term is always a literal, even if its text equals an internal
		// operator marker. This is the legacy simple-search representation.
		terms.emplace_back([&] { target.WriteMetaDataSearchParam(expression[0]); });
	} else if (!expression.IsEmpty()) {
		terms.emplace_back([&] {
			for (const auto &term : expression) {
				if (term == SEARCHOPTOK_AND) {
					target.WriteBooleanAND();
				} else if (term == SEARCHOPTOK_OR) {
					target.WriteBooleanOR();
				} else if (term == SEARCHOPTOK_NOT) {
					target.WriteBooleanNOT();
				} else {
					target.WriteMetaDataSearchParam(term);
				}
			}
		});
	}
	if (!params.typeText.IsEmpty()) {
		terms.emplace_back([&] {
			wxString type = params.typeText;
			// Preserve the historical simple-search archive/CD-image mapping.
			if (expression.GetCount() <= 1 &&
				(type == ED2KFTSTR_ARCHIVE || type == ED2KFTSTR_CDIMAGE)) {
				type = ED2KFTSTR_PROGRAM;
			}
			target.WriteMetaDataSearchParamASCII(FT_FILETYPE, type);
		});
	}
	auto numeric = [&](uint8 tag, uint8 op, uint64 value) {
		if (value > 0) {
			terms.emplace_back(
				[&, tag, op, value] { target.WriteMetaDataSearchParam(tag, op, value); });
		}
	};
	auto text = [&](uint8 tag, const wxString &value) {
		if (!value.IsEmpty()) {
			terms.emplace_back([&, tag, value] { target.WriteMetaDataSearchParam(tag, value); });
		}
	};
	numeric(FT_FILESIZE, ED2K_SEARCH_OP_GREATER, params.minSize);
	numeric(FT_FILESIZE, ED2K_SEARCH_OP_LESS, params.maxSize);
	numeric(FT_SOURCES, ED2K_SEARCH_OP_GREATER, params.availability);
	text(FT_FILEFORMAT, params.extension);
	numeric(FT_MEDIA_BITRATE, ED2K_SEARCH_OP_GREATER_EQUAL, params.minBitrateKbps);
	numeric(FT_MEDIA_LENGTH, ED2K_SEARCH_OP_GREATER_EQUAL, params.minLength);
	text(FT_MEDIA_CODEC, params.codec);
	text(FT_MEDIA_TITLE, params.title);
	text(FT_MEDIA_ALBUM, params.album);
	text(FT_MEDIA_ARTIST, params.artist);

	// Preserve both legacy layouts byte for byte when metadata is absent.
	if (expression.GetCount() <= 1) {
		for (size_t i = 0; i < terms.size(); ++i) {
			if (i + 1 < terms.size()) {
				target.WriteBooleanAND();
			}
			terms[i]();
		}
	} else if (!terms.empty()) {
		for (size_t i = 1; i < terms.size(); ++i) {
			target.WriteBooleanAND();
		}
		for (const auto &term : terms) {
			term();
		}
	}
}

#endif
