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

#include "SearchPacket.h"
#include "MemFile.h"
#include "Logger.h"
#include "kademlia/kademlia/SearchManager.h"
#include <common/Format.h>
#include <protocol/Protocols.h>
#include <tags/FileTags.h>

#include "SearchExpr.h"

#include "Scanner.h"
void LexInit(const wxString &pszInput);
void LexFree();

#include "Parser.hpp"
int yyerror(wxString errstr);

static wxString s_strCurKadKeyword;

static CSearchExpr _SearchExpr;

wxArrayString _astrParserErrors;

// Helper function for lexer.
void ParsedSearchExpression(const CSearchExpr *pexpr)
{
	int iOpAnd = 0;
	int iOpOr = 0;
	int iOpNot = 0;

	for (unsigned int i = 0; i < pexpr->m_aExpr.GetCount(); i++) {
		const wxString &str = pexpr->m_aExpr[i];
		if (str == SEARCHOPTOK_AND) {
			iOpAnd++;
		} else if (str == SEARCHOPTOK_OR) {
			iOpOr++;
		} else if (str == SEARCHOPTOK_NOT) {
			iOpNot++;
		}
	}

	// This limit (plus the operators added later) has to match the limit in
	// CreateSearchExpressionTree: one each for Type, MinSize, MaxSize, Avail, Extension,
	// Complete sources, Codec, Bitrate, Length, Title, Album and Artist -- 12 in total.
	if (iOpAnd + iOpOr + iOpNot > 10) {
		yyerror("Search expression is too complex");
	}

	_SearchExpr.m_aExpr.Empty();

	// optimize search expression, if no OR nor NOT specified
	if (iOpAnd > 0 && iOpOr == 0 && iOpNot == 0) {
		// Figure out if we can use a better keyword than the one the user selected: most users
		// search like "The oxymoronaccelerator 2", which would ask the node indexing "the" --
		// higher traffic for such nodes and a viable target for attackers. Asking the node that
		// indexes the rare keyword gives the same or better results, so keywords are rearranged
		// on the assumption that longer keywords are rarer.
		if (/*thePrefs::GetRearrangeKadSearchKeywords() &&*/ !s_strCurKadKeyword.IsEmpty()) {
			for (unsigned int i = 0; i < pexpr->m_aExpr.GetCount(); i++) {
				if (pexpr->m_aExpr[i] != SEARCHOPTOK_AND) {
					if (pexpr->m_aExpr[i] != s_strCurKadKeyword &&
						pexpr->m_aExpr[i].find_first_of(
							Kademlia::CSearchManager::GetInvalidKeywordChars()) ==
							wxString::npos &&
						pexpr->m_aExpr[i].Find('"') !=
							0 // no quoted expressions as keyword
						&& pexpr->m_aExpr[i].length() >= 3 &&
						s_strCurKadKeyword.length() < pexpr->m_aExpr[i].length()) {
						s_strCurKadKeyword = pexpr->m_aExpr[i];
					}
				}
			}
		}
		wxString strAndTerms;
		for (unsigned int i = 0; i < pexpr->m_aExpr.GetCount(); i++) {
			if (pexpr->m_aExpr[i] != SEARCHOPTOK_AND) {
				// Minor optimization: because the Kad keyword was added to the
				// boolean search expression, remove it here (and only here) -- the
				// whole expression contains only (implicit) ANDed strings.
				if (pexpr->m_aExpr[i] != s_strCurKadKeyword) {
					if (!strAndTerms.IsEmpty()) {
						strAndTerms += ' ';
					}
					strAndTerms += pexpr->m_aExpr[i];
				}
			}
		}
		wxASSERT(_SearchExpr.m_aExpr.GetCount() == 0);
		_SearchExpr.m_aExpr.Add(strAndTerms);
	} else {
		if (pexpr->m_aExpr.GetCount() != 1 || pexpr->m_aExpr[0] != s_strCurKadKeyword)
			_SearchExpr.Add(pexpr);
	}
}

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

	void WriteMetaDataSearchParam(const wxString &pszMetaTagID, const wxString &rstrValue)
	{
		m_data->WriteUInt8(2);                        // string parameter type
		m_data->WriteString(rstrValue, m_eStrEncode); // string value
		m_data->WriteString(pszMetaTagID);            // meta tag ID
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

	void WriteMetaDataSearchParam(const wxString &pszMetaTagID, uint8_t uOperator, uint64_t value)
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
		m_data->WriteUInt8(uOperator);     // comparison operator
		m_data->WriteString(pszMetaTagID); // meta tag ID
	}

protected:
	CMemFile *m_data;
	EUtf8Str m_eStrEncode;
	bool m_supports64bit;
	bool &m_using64bit;
};

CSmartPtr<CMemFile> CreateSearchData(
	CSearchList::CSearchParams &params, SearchType type, bool supports64bit, bool &packetUsing64bit)
{
	// Count the number of used parameters
	unsigned int parametercount = 0;
	if (!params.typeText.IsEmpty())
		++parametercount;
	if (params.minSize > 0)
		++parametercount;
	if (params.maxSize > 0)
		++parametercount;
	if (params.availability > 0)
		++parametercount;
	if (!params.extension.IsEmpty())
		++parametercount;
	if (params.minBitrateKbps > 0)
		++parametercount;

	wxString typeText = params.typeText;
	if (typeText == ED2KFTSTR_ARCHIVE) {
		// eDonkeyHybrid 0.48 uses type "Pro" for archives files
		// www.filedonkey.com uses type "Pro" for archives files
		typeText = ED2KFTSTR_PROGRAM;
	} else if (typeText == ED2KFTSTR_CDIMAGE) {
		// eDonkeyHybrid 0.48 uses *no* type for iso/nrg/cue/img files
		// www.filedonkey.com uses type "Pro" for CD-image files
		typeText = ED2KFTSTR_PROGRAM;
	}

	// Must write parametercount - 1 parameter headers
	CSmartPtr<CMemFile> data(new CMemFile(100));

	_astrParserErrors.Empty();
	_SearchExpr.m_aExpr.Empty();

	s_strCurKadKeyword.Clear();
	if (type == KadSearch) {
		wxASSERT(!params.strKeyword.IsEmpty());
		s_strCurKadKeyword = params.strKeyword;
	}

	LexInit(params.searchString);
	int iParseResult = yyparse();
	LexFree();

	if (_astrParserErrors.GetCount() > 0) {
		for (unsigned int i = 0; i < _astrParserErrors.GetCount(); ++i) {
			AddLogLineNS(CFormat(_("Error %u: %s\n")) % i % _astrParserErrors[i]);
		}

		return CSmartPtr<CMemFile>(nullptr);
	}

	if (iParseResult != 0) {
		_astrParserErrors.Add(CFormat("Undefined error %i on search expression") % iParseResult);

		return CSmartPtr<CMemFile>(nullptr);
	}

	if (type == KadSearch && s_strCurKadKeyword != params.strKeyword) {
		AddDebugLogLineN(logSearch,
			CFormat("Keyword was rearranged, using '%s' instead of '%s'") % s_strCurKadKeyword %
				params.strKeyword);
		params.strKeyword = s_strCurKadKeyword;
	}

	parametercount += _SearchExpr.m_aExpr.GetCount();

	/* Leave the unicode comment there, please... */
	CSearchExprTarget target(data.get(),
		true /*I assume everyone is unicoded */ ? utf8strRaw : utf8strNone,
		supports64bit,
		packetUsing64bit);

	unsigned int iParameterCount = 0;
	if (_SearchExpr.m_aExpr.GetCount() <= 1) {
		// lugdunummaster requested that searches without OR or NOT operators, and hence with no
		// more expressions than the string itself, be sent using a series of ANDed terms,
		// intersecting the ANDs on the terms (but prepending them) instead of putting the boolean
		// tree at the start like other searches. This type of search is supposed to take less load
		// on servers. Go figure.
		//
		// input:      "a" AND min=1 AND max=2
		// instead of: AND AND "a" min=1 max=2
		// we use:     AND "a" AND min=1 max=2

		if (_SearchExpr.m_aExpr.GetCount() > 0) {
			if (++iParameterCount < parametercount) {
				target.WriteBooleanAND();
			}
			target.WriteMetaDataSearchParam(_SearchExpr.m_aExpr[0]);
		}

		if (!typeText.IsEmpty()) {
			if (++iParameterCount < parametercount) {
				target.WriteBooleanAND();
			}
			// Type is always ascii string
			target.WriteMetaDataSearchParamASCII(FT_FILETYPE, typeText);
		}

		if (params.minSize > 0) {
			if (++iParameterCount < parametercount) {
				target.WriteBooleanAND();
			}
			target.WriteMetaDataSearchParam(FT_FILESIZE, ED2K_SEARCH_OP_GREATER, params.minSize);
		}

		if (params.maxSize > 0) {
			if (++iParameterCount < parametercount) {
				target.WriteBooleanAND();
			}
			target.WriteMetaDataSearchParam(FT_FILESIZE, ED2K_SEARCH_OP_LESS, params.maxSize);
		}

		if (params.availability > 0) {
			if (++iParameterCount < parametercount) {
				target.WriteBooleanAND();
			}
			target.WriteMetaDataSearchParam(
				FT_SOURCES, ED2K_SEARCH_OP_GREATER, params.availability);
		}

		if (!params.extension.IsEmpty()) {
			if (++iParameterCount < parametercount) {
				target.WriteBooleanAND();
			}
			target.WriteMetaDataSearchParam(FT_FILEFORMAT, params.extension);
		}

		if (params.minBitrateKbps > 0) {
			if (++iParameterCount < parametercount) {
				target.WriteBooleanAND();
			}
			target.WriteMetaDataSearchParam(
				FT_MEDIA_BITRATE, ED2K_SEARCH_OP_GREATER_EQUAL, params.minBitrateKbps);
		}

// Before enabling these filters, follow the bitrate term above: numeric FT_* IDs
// on both networks and GREATER_EQUAL for inclusive minimums (not legacy string tags).
#if 0
		if (complete > 0){
			if (++iParameterCount < parametercount) {
				target.WriteBooleanAND();
			}
			target.WriteMetaDataSearchParam(FT_COMPLETE_SOURCES, ED2K_SEARCH_OP_GREATER, complete);
		}

		if (minLength > 0){
			if (++iParameterCount < parametercount) {
				target.WriteBooleanAND();
			}
			target.WriteMetaDataSearchParam(type == KadSearch ? TAG_MEDIA_LENGTH : FT_ED2K_MEDIA_LENGTH, ED2K_SEARCH_OP_GREATER, minLength);
		}

		if (!codec.IsEmpty()){
			if (++iParameterCount < parametercount) {
				target.WriteBooleanAND();
			}
			target.WriteMetaDataSearchParam(type == KadSearch ? TAG_MEDIA_CODEC : FT_ED2K_MEDIA_CODEC, codec);
		}

		if (!title.IsEmpty()){
			if (++iParameterCount < parametercount) {
				target.WriteBooleanAND();
			}
			target.WriteMetaDataSearchParam(type == KadSearch ? TAG_MEDIA_TITLE : FT_ED2K_MEDIA_TITLE, title);
		}

		if (!album.IsEmpty()){
			if (++iParameterCount < parametercount) {
				target.WriteBooleanAND();
			}
			target.WriteMetaDataSearchParam(type == KadSearch ? TAG_MEDIA_ALBUM : FT_ED2K_MEDIA_ALBUM, album);
		}

		if (!artist.IsEmpty()){
			if (++iParameterCount < parametercount) {
				target.WriteBooleanAND();
			}
			target.WriteMetaDataSearchParam(type == KadSearch ? TAG_MEDIA_ARTIST : FT_ED2K_MEDIA_ARTIST, artist);
		}
#endif // 0

		// If this assert fails... we're seriously fucked up

		wxASSERT(iParameterCount == parametercount);

	} else {
		if (!params.extension.IsEmpty()) {
			if (++iParameterCount < parametercount) {
				target.WriteBooleanAND();
			}
		}

		if (params.availability > 0) {
			if (++iParameterCount < parametercount) {
				target.WriteBooleanAND();
			}
		}

		if (params.maxSize > 0) {
			if (++iParameterCount < parametercount) {
				target.WriteBooleanAND();
			}
		}

		if (params.minSize > 0) {
			if (++iParameterCount < parametercount) {
				target.WriteBooleanAND();
			}
		}

		if (!typeText.IsEmpty()) {
			if (++iParameterCount < parametercount) {
				target.WriteBooleanAND();
			}
		}

		if (params.minBitrateKbps > 0) {
			if (++iParameterCount < parametercount) {
				target.WriteBooleanAND();
			}
		}

// Keep these AND headers in sync with the filters and parameter count when enabling them.
#if 0
		if (complete > 0){
			if (++iParameterCount < parametercount) {
				target.WriteBooleanAND();
			}
		}

		if (minLength > 0) {
			if (++iParameterCount < parametercount) {
				target.WriteBooleanAND();
			}
		}

		if (!codec.IsEmpty()){
			if (++iParameterCount < parametercount) {
				target.WriteBooleanAND();
			}
		}

		if (!title.IsEmpty()){
			if (++iParameterCount < parametercount) {
				target.WriteBooleanAND();
			}
		}

		if (!album.IsEmpty()) {
			if (++iParameterCount < parametercount) {
				target.WriteBooleanAND();
			}
		}

		if (!artist.IsEmpty()) {
			if (++iParameterCount < parametercount) {
				target.WriteBooleanAND();
			}
		}
#endif // 0

		// As above, if this fails, we're seriously fucked up.
		wxASSERT(iParameterCount + _SearchExpr.m_aExpr.GetCount() == parametercount);

		for (unsigned int j = 0; j < _SearchExpr.m_aExpr.GetCount(); ++j) {
			if (_SearchExpr.m_aExpr[j] == SEARCHOPTOK_AND) {
				target.WriteBooleanAND();
			} else if (_SearchExpr.m_aExpr[j] == SEARCHOPTOK_OR) {
				target.WriteBooleanOR();
			} else if (_SearchExpr.m_aExpr[j] == SEARCHOPTOK_NOT) {
				target.WriteBooleanNOT();
			} else {
				target.WriteMetaDataSearchParam(_SearchExpr.m_aExpr[j]);
			}
		}

		if (!params.typeText.IsEmpty()) {
			// Type is always ASCII string
			target.WriteMetaDataSearchParamASCII(FT_FILETYPE, params.typeText);
		}

		if (params.minSize > 0) {
			target.WriteMetaDataSearchParam(FT_FILESIZE, ED2K_SEARCH_OP_GREATER, params.minSize);
		}

		if (params.maxSize > 0) {
			target.WriteMetaDataSearchParam(FT_FILESIZE, ED2K_SEARCH_OP_LESS, params.maxSize);
		}

		if (params.availability > 0) {
			target.WriteMetaDataSearchParam(
				FT_SOURCES, ED2K_SEARCH_OP_GREATER, params.availability);
		}

		if (!params.extension.IsEmpty()) {
			target.WriteMetaDataSearchParam(FT_FILEFORMAT, params.extension);
		}

		if (params.minBitrateKbps > 0) {
			target.WriteMetaDataSearchParam(
				FT_MEDIA_BITRATE, ED2K_SEARCH_OP_GREATER_EQUAL, params.minBitrateKbps);
		}

// As above, follow the bitrate term: numeric FT_* IDs on both networks and
// GREATER_EQUAL for inclusive minimums when enabling these filters.
#if 0
		if (complete > 0) {
			target.WriteMetaDataSearchParam(FT_COMPLETE_SOURCES, ED2K_SEARCH_OP_GREATER, pParams->uComplete);
		}

		if (minLength > 0) {
			target.WriteMetaDataSearchParam(type == KadSearch ? TAG_MEDIA_LENGTH : FT_ED2K_MEDIA_LENGTH, ED2K_SEARCH_OP_GREATER, minLength);
		}

		if (!codec.IsEmpty()) {
			target.WriteMetaDataSearchParam(type == KadSearch ? TAG_MEDIA_CODEC : FT_ED2K_MEDIA_CODEC, codec);
		}

		if (!title.IsEmpty()) {
			target.WriteMetaDataSearchParam(type == KadSearch ? TAG_MEDIA_TITLE : FT_ED2K_MEDIA_TITLE, title);
		}

		if (!album.IsEmpty()) {
			target.WriteMetaDataSearchParam(type == KadSearch ? TAG_MEDIA_ALBUM : FT_ED2K_MEDIA_ALBUM, album);
		}

		if (!artist.IsEmpty()) {
			target.WriteMetaDataSearchParam(type == KadSearch ? TAG_MEDIA_ARTIST : FT_ED2K_MEDIA_ARTIST, artist);
		}

#endif // 0
	}

	// Packet ready to go.
	return data;
}
