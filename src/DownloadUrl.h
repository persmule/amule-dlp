//
// This file is part of the aMule Project.
//
// Copyright (c) 2026 aMule Team ( https://amule-org.github.io )
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

#ifndef AMULE_DOWNLOADURL_H
#define AMULE_DOWNLOADURL_H

#include <wx/string.h>

// For diagnostics only: requests and conditional-download identity must keep the
// original URL. Only the scheme and host/port are needed in logs; credentials
// can appear in user info, paths, query strings and fragments.
inline wxString DownloadUrlForLog(const wxString &url)
{
	const auto scheme = url.find("://");
	if (scheme == wxString::npos) {
		return wxEmptyString;
	}
	const wxString authority = url.Mid(scheme + 3).BeforeFirst('/').AfterLast('@');
	return url.Left(scheme + 3) + authority.BeforeFirst('?').BeforeFirst('#');
}

// Preserve backend failure reasons, including DNS, TLS and timeout diagnostics,
// while sanitizing any request or redirect URLs embedded in them.
inline wxString DownloadErrorForLog(wxString error)
{
	wxString::size_type pos = 0;
	while ((pos = error.find("://", pos)) != wxString::npos) {
		auto start = pos;
		while (start > 0) {
			const auto ch = error[start - 1];
			if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
				    (ch >= '0' && ch <= '9') || ch == '+' || ch == '-' || ch == '.')) {
				break;
			}
			--start;
		}
		auto end = pos + 3;
		while (end < error.length() && error[end] > 0x20) {
			++end;
		}
		// Quotes and parentheses can occur inside URL paths. Consume the whole
		// token, preserving only trailing punctuation around a quoted URL.
		while (end > pos + 3 && wxString("\"')>,;:.").Contains(error[end - 1])) {
			--end;
		}
		const wxString safe = DownloadUrlForLog(error.Mid(start, end - start));
		error.replace(start, end - start, safe);
		pos = start + safe.length();
	}
	return error;
}

#endif
