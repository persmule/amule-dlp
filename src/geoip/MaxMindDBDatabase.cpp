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

#include "MaxMindDBDatabase.h"

#include "CCtypeAsciiScope.h"
#include "Logger.h"
#include <common/Format.h>
#include <common/StringFunctions.h> // unicode2char
#include <wx/intl.h>                // _() gettext macro

#include <filesystem>
#include <utility>

#ifdef __WXMSW__
#include <wx/msw/wrapwin.h>
#endif

CMaxMindDBDatabase::CMaxMindDBDatabase()
: m_mmdb{}
, m_isOpen(false)
{
}

CMaxMindDBDatabase::~CMaxMindDBDatabase()
{
	Close();
}

bool CMaxMindDBDatabase::Open(const wxString &path)
{
	Close();

	int status = MMDB_open(unicode2char(path), MMDB_MODE_MMAP, &m_mmdb);
	if (status != MMDB_SUCCESS) {
		AddLogLineC(CFormat(_("Failed to open MaxMindDB database '%s': %s")) % path %
			    wxString::FromUTF8(MMDB_strerror(status)));
		return false;
	}

	m_isOpen = true;

	// Opening is not enough: a MaxMind .tar.gz saved unextracted opens (the metadata sits near
	// its end), but every lookup fails. Probe an address any country database knows.
	if (GetCountryCode("8.8.8.8").IsEmpty()) {
		AddLogLineC(
			CFormat(_("Failed to open MaxMindDB database '%s': %s")) % path % "no country data");
		Close();
		return false;
	}
	m_path = path;
	return true;
}

bool CMaxMindDBDatabase::Replace(const wxString &downloadPath, const wxString &path)
{
	CMaxMindDBDatabase candidate;
	if (!candidate.Open(downloadPath)) {
		return false;
	}

	// Use a native replacement: wxRenameFile(overwrite=true)
	// may remove the destination first or fall back to copying on failure.
	std::error_code error;
#ifdef __WXMSW__
	// Windows cannot replace a memory-mapped file. Validation is complete before
	// closing either mapping; a failed rename leaves the original file in place.
	const bool wasOpen = IsOpen();
	const wxString originalPath = m_path;
	candidate.Close();
	Close();
	if (!MoveFileExW(downloadPath.wc_str(), path.wc_str(), MOVEFILE_REPLACE_EXISTING)) {
		error = std::error_code(GetLastError(), std::system_category());
	}
#else
	const std::filesystem::path source(downloadPath.fn_str().data());
	const std::filesystem::path destination(path.fn_str().data());
	std::filesystem::rename(source, destination, error);
#endif
	if (error) {
		AddLogLineC(CFormat(_("Failed to replace MaxMindDB database '%s': %s")) % path %
			    wxString::FromUTF8(error.message().c_str()));
#ifdef __WXMSW__
		if (wasOpen) {
			Open(originalPath);
		}
#endif
		return false;
	}

#ifdef __WXMSW__
	return Open(path);
#else
	// Keep the validated mapping instead of reopening the renamed file. The old
	// mapping is released by candidate's destructor after the swap.
	std::swap(m_mmdb, candidate.m_mmdb);
	std::swap(m_isOpen, candidate.m_isOpen);
	m_path = path;
	return true;
#endif
}

void CMaxMindDBDatabase::Close()
{
	if (m_isOpen) {
		MMDB_close(&m_mmdb);
		m_isOpen = false;
	}
}

wxString CMaxMindDBDatabase::GetCountryCode(const wxString &ip) const
{
	if (!m_isOpen) {
		return wxEmptyString;
	}

	int gai_error = 0;
	int mmdb_error = 0;
	MMDB_lookup_result_s result =
		MMDB_lookup_string(const_cast<MMDB_s *>(&m_mmdb), unicode2char(ip), &gai_error, &mmdb_error);

	if (gai_error || mmdb_error != MMDB_SUCCESS || !result.found_entry) {
		return wxEmptyString;
	}

	// GeoLite2-Country and equivalents nest the ISO code under
	// country -> iso_code (UTF-8 string).
	MMDB_entry_data_s entry_data;
	int status = MMDB_get_value(&result.entry, &entry_data, "country", "iso_code", NULL);
	if (status != MMDB_SUCCESS || !entry_data.has_data || entry_data.type != MMDB_DATA_TYPE_UTF8_STRING ||
		entry_data.data_size == 0) {
		return wxEmptyString;
	}

	// utf8_string is NOT NUL-terminated; data_size is the byte length.
	wxString code = wxString::FromUTF8(entry_data.utf8_string, entry_data.data_size);

	// Lock LC_CTYPE to "C" so the lowercase ISO 3166-1 alpha-2 code stays ASCII. In tr_TR,
	// wxString::Lower() folds 'I' to U+0131 (dotless i) instead of 'i', which would turn "IT"
	// into a name that misses the embedded flag artwork lookup in CCountryFlags.
	// SVN r10697 fixed the same bug in the old libGeoIP path in 2011; the fix was lost when
	// GeoIP was replaced by libmaxminddb. See amule forum topic 19398.
	CCtypeAsciiScope scope;
	return code.Lower();
}
