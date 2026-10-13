// This file is part of the aMule Project.
// Copyright (c) 2003-2026 aMule Team ( https://amule-org.github.io )
// Licensed under the GNU General Public License, version 2 or later.

#include <muleunit/test.h>
#include "geoip/MaxMindDBDatabase.h"

#include <wx/ffile.h>
#include <wx/filefn.h>
#include <wx/filename.h>

#include <stdexcept>
#include <string>

using namespace muleunit;
DECLARE_SIMPLE(MaxMindDBUpdate)

namespace
{
// A self-contained MMDB with one IPv4 search node. Both branches resolve to
// the supplied country. Metadata and data use MMDB's map/string/integer encoding.
std::string Database(const std::string &country)
{
	std::string bytes("\0\0\x11\0\0\x11", 6);
	bytes.append(16, '\0');
	auto string = [](const std::string &value) {
		return std::string(1, static_cast<char>(0x40 + value.size())) + value;
	};
	bytes += "\xe1" + string("country") + "\xe1" + string("iso_code") + string(country);
	bytes += std::string("\xab\xcd\xefMaxMind.com", 14);
	bytes += '\xe9'; // Nine metadata fields.
	bytes += string("node_count") + std::string("\xc1\x01", 2);
	bytes += string("record_size") + std::string("\xa1\x18", 2);
	bytes += string("ip_version") + std::string("\xa1\x04", 2);
	bytes += string("database_type") + string("Test-Country");
	bytes += string("languages") + std::string("\x01\x04", 2) + string("en");
	bytes += string("binary_format_major_version") + std::string("\xa1\x02", 2);
	bytes += string("binary_format_minor_version") + std::string("\xa1\0", 2);
	bytes += string("build_epoch") + std::string("\x01\x02\x01", 3);
	bytes += string("description") + "\xe1" + string("en") + string("Update test");
	return bytes;
}

struct Files
{
	wxString path = wxFileName::CreateTempFileName("geoip-update-");
	wxString download = path + ".download";
	~Files()
	{
		if (wxFileExists(download)) {
			wxRemoveFile(download);
		}
		wxRemoveFile(path);
	}
	void Write(const wxString &name, const std::string &bytes)
	{
		wxFFile file(name, "wb");
		if (!file.IsOpened() || file.Write(bytes.data(), bytes.size()) != bytes.size()) {
			throw std::runtime_error("Cannot write MMDB fixture");
		}
	}
};
} // namespace

TEST(MaxMindDBUpdate, InvalidDownloadPreservesFileAndResolver)
{
	Files files;
	files.Write(files.path, Database("US"));
	files.Write(files.download, "truncated or HTML download");
	CMaxMindDBDatabase db;
	ASSERT_TRUE(db.Open(files.path));
	ASSERT_FALSE(db.Replace(files.download, files.path));
	ASSERT_EQUALS(wxString("us"), db.GetCountryCode("8.8.8.8"));
	CMaxMindDBDatabase reopened;
	ASSERT_TRUE(reopened.Open(files.path));
	ASSERT_EQUALS(wxString("us"), reopened.GetCountryCode("8.8.8.8"));
}

TEST(MaxMindDBUpdate, DatabaseWithoutCountryDataPreservesFileAndResolver)
{
	Files files;
	files.Write(files.path, Database("US"));
	files.Write(files.download, Database(""));
	CMaxMindDBDatabase db;
	ASSERT_TRUE(db.Open(files.path));
	ASSERT_FALSE(db.Replace(files.download, files.path));
	ASSERT_EQUALS(wxString("us"), db.GetCountryCode("8.8.8.8"));
	CMaxMindDBDatabase reopened;
	ASSERT_TRUE(reopened.Open(files.path));
	ASSERT_EQUALS(wxString("us"), reopened.GetCountryCode("8.8.8.8"));
}

TEST(MaxMindDBUpdate, RenameFailurePreservesFileAndResolver)
{
	Files files;
	files.Write(files.path, Database("US"));
	files.Write(files.download, Database("CA"));
	CMaxMindDBDatabase db;
	ASSERT_TRUE(db.Open(files.path));
	ASSERT_FALSE(db.Replace(files.download, files.path + "/missing/database.mmdb"));
	ASSERT_TRUE(wxFileExists(files.download));
	ASSERT_EQUALS(wxString("us"), db.GetCountryCode("8.8.8.8"));
	CMaxMindDBDatabase reopened;
	ASSERT_TRUE(reopened.Open(files.path));
	ASSERT_EQUALS(wxString("us"), reopened.GetCountryCode("8.8.8.8"));
}

TEST(MaxMindDBUpdate, ValidDownloadReplacesFileAndResolver)
{
	Files files;
	files.Write(files.path, Database("US"));
	files.Write(files.download, Database("CA"));
	CMaxMindDBDatabase db;
	ASSERT_TRUE(db.Open(files.path));
	ASSERT_TRUE(db.Replace(files.download, files.path));
	ASSERT_FALSE(wxFileExists(files.download));
	ASSERT_EQUALS(wxString("ca"), db.GetCountryCode("8.8.8.8"));
	CMaxMindDBDatabase reopened;
	ASSERT_TRUE(reopened.Open(files.path));
	ASSERT_EQUALS(wxString("ca"), reopened.GetCountryCode("8.8.8.8"));
}

TEST(MaxMindDBUpdate, ValidDownloadInstallsWithoutExistingDatabase)
{
	Files files;
	wxRemoveFile(files.path);
	files.Write(files.download, Database("CA"));
	CMaxMindDBDatabase db;
	ASSERT_TRUE(db.Replace(files.download, files.path));
	ASSERT_TRUE(db.IsOpen());
	ASSERT_EQUALS(wxString("ca"), db.GetCountryCode("8.8.8.8"));
}
