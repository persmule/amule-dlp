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

#include <muleunit/test.h>
#include "DownloadUrl.h" // Needed for DownloadUrlForLog, DownloadErrorForLog

using namespace muleunit;
DECLARE_SIMPLE(DownloadUrl)

TEST(DownloadUrl, RemovesCredentialsPathsQueriesAndFragments)
{
	ASSERT_EQUALS(wxString("https://download.maxmind.com"),
		DownloadUrlForLog("https://download.maxmind.com/app/geoip_download?license_key=secret"));
	ASSERT_EQUALS(wxString("https://geo.example"),
		DownloadUrlForLog("https://user:password@geo.example/path-token/db?token=secret#fragment"));
	ASSERT_EQUALS(wxString("https://geo.example"),
		DownloadUrlForLog("https://geo.example/signed-path-token/db"));
}

TEST(DownloadUrl, KeepsSchemeHostAndPortWithoutValidation)
{
	ASSERT_EQUALS(wxString("HTTPS://[::1]:8443"),
		DownloadUrlForLog("HTTPS://user:p%40ss@[::1]:8443/a%20b/db?token=secret"));
	ASSERT_EQUALS(wxString("https://[fe80::1%25eth0]:8443"),
		DownloadUrlForLog("https://user:pass@[fe80::1%25eth0]:8443/db?secret"));
	ASSERT_EQUALS(wxString("ftp://example.com:service"),
		DownloadUrlForLog("ftp://user:pass@example.com:service/path"));
	ASSERT_EQUALS(wxString("custom+scheme://unusual_host"),
		DownloadUrlForLog("custom+scheme://unusual_host/path"));
	ASSERT_EQUALS(wxString("https://example.com"), DownloadUrlForLog("https://example.com"));
	ASSERT_EQUALS(wxString(""), DownloadUrlForLog("token=secret"));
	ASSERT_EQUALS(wxString(""), DownloadUrlForLog(""));
}

TEST(DownloadUrl, RemovesMalformedUserInfoBeforeQueryAndFragment)
{
	ASSERT_EQUALS(wxString("http://host"), DownloadUrlForLog("http://user:pa?ss@host/db"));
	ASSERT_EQUALS(wxString("http://host"), DownloadUrlForLog("http://user:pa#ss@host/db"));
	ASSERT_EQUALS(
		wxString("https://host"), DownloadUrlForLog("https://user:first@second@host/db?secret"));
}

TEST(DownloadUrl, HandlesAtSignsWithoutExposingPathsOrQueries)
{
	ASSERT_EQUALS(wxString("https://example.com"),
		DownloadUrlForLog("https://example.com/path@name/db?token=secret@other.example"));
	ASSERT_EQUALS(wxString("https://example.com"),
		DownloadUrlForLog("https://us%3Fer:p%23ss@example.com/db?token=secret"));
	ASSERT_EQUALS(wxString("https://example.com"), DownloadUrlForLog("https://example.com?secret#token"));
	ASSERT_EQUALS(wxString("https://example.com"), DownloadUrlForLog("https://example.com#secret?token"));
}

TEST(DownloadUrl, KeepsBackendFailureReasons)
{
	for (const auto *reason : { "Could not resolve host: example.com",
		     "SSL certificate problem: certificate has expired",
		     "Operation timed out after 30000 milliseconds",
		     "Server returned nothing (no headers, no data)",
		     "" }) {
		ASSERT_EQUALS(wxString(reason), DownloadErrorForLog(reason));
	}
}

TEST(DownloadUrl, SanitizesRequestAndRedirectUrlsInFailureReasons)
{
	ASSERT_EQUALS(wxString("Failed https://host after redirect to http://other:8080: TLS error"),
		DownloadErrorForLog("Failed https://user:pa?ss@host/path-secret?key=secret "
				    "after redirect to http://user:pass@other:8080/path-token: TLS error"));
	ASSERT_EQUALS(wxString("Failed 'https://host': timeout (https://[::1]:8443)"),
		DownloadErrorForLog("Failed 'https://user:pass@host/token?key=secret': "
				    "timeout (https://[::1]:8443/token#secret)"));
	ASSERT_EQUALS(wxString("Failed \"https://host\": TLS error"),
		DownloadErrorForLog("Failed \"https://host/path-secret?key=secret\": TLS error"));
}

TEST(DownloadUrl, HidesTokensAfterPunctuationInsidePaths)
{
	ASSERT_EQUALS(wxString("Failed https://host: timeout"),
		DownloadErrorForLog("Failed https://host/path'with(secret)/token?key=secret: timeout"));
	ASSERT_EQUALS(wxString("Failed <https://host>: timeout"),
		DownloadErrorForLog("Failed <https://user:pass@host/token?key=secret>: timeout"));
	ASSERT_EQUALS(wxString("https://host"), DownloadErrorForLog("https://host/path-token"));
}
