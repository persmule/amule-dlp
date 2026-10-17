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
#include "SearchPacketWriter.h"
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
	// Codec, Bitrate, Length, Title, Album and Artist -- 11 in total.
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

CSmartPtr<CMemFile> CreateSearchData(
	CSearchList::CSearchParams &params, SearchType type, bool supports64bit, bool &packetUsing64bit)
{
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

	CSearchExprTarget target(data.get(), utf8strRaw, supports64bit, packetUsing64bit);
	WriteSearchPacket(params, _SearchExpr.m_aExpr, target);

	// Packet ready to go.
	return data;
}
