// This file is part of the aMule Project.
// Copyright (c) 2003-2026 aMule Team ( https://amule-org.github.io )
// Licensed under the GNU General Public License, version 2 or later.

#ifndef SEARCHPACKET_H
#define SEARCHPACKET_H

#include "SearchList.h"
#include <wx/arrstr.h>

/** Create a search packet for the given network, reporting parser failures below. */
CSmartPtr<CMemFile> CreateSearchData(
	CSearchList::CSearchParams &params, SearchType type, bool supports64bit, bool &packetUsing64bit);

// Parser diagnostics from the most recent CreateSearchData call.
extern wxArrayString _astrParserErrors;

#endif // SEARCHPACKET_H
