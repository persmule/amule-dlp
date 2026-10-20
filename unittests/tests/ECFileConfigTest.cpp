// This file is part of the aMule Project.
// Copyright (c) 2026 aMule Team ( https://amule-org.github.io )
// Licensed under the GNU General Public License, version 2 or later.

#include <muleunit/test.h>

#include <initializer_list>

#include "libs/ec/cpp/ECFileConfig.h"

using namespace muleunit;

// Keep configuration-directory selection deterministic and independent of user files.
wxString GetConfigDir(const wxString &)
{
	return wxFileName::DirName(wxGetHomeDir()).GetPathWithSep() + "test-config" +
	       wxFileName::GetPathSeparator();
}

DECLARE_SIMPLE(ECFileConfig)

TEST(ECFileConfig, BareNamesUseConfigDirectory)
{
	for (const wxString &name : { wxString("remote.conf"), wxString(""), wxString("~") }) {
		ASSERT_EQUALS(GetConfigDir(name) + name, FinalizeFilename(name));
	}
}

TEST(ECFileConfig, ExplicitRelativePathsArePreserved)
{
	for (const wxString &path : { wxString("./remote.conf"),
		     wxString("../remote.conf"),
		     wxString("subdir/remote.conf"),
		     wxString("subdir/../remote.conf") }) {
		ASSERT_EQUALS(path, FinalizeFilename(path));
	}
}

TEST(ECFileConfig, SelectedConfigDirectoryIsUsedForBareNames)
{
	const wxString directory = wxFileName::DirName("portable-config").GetPathWithSep();
	ASSERT_EQUALS(directory + "custom.conf", FinalizeFilename("custom.conf", directory));
	ASSERT_EQUALS(wxString("./custom.conf"), FinalizeFilename("./custom.conf", directory));
	ASSERT_EQUALS(wxGetHomeDir() + "/custom.conf", FinalizeFilename("~/custom.conf", directory));
}

TEST(ECFileConfig, RootedPathsArePreserved)
{
	ASSERT_EQUALS(wxString("/remote.conf"), FinalizeFilename("/remote.conf"));
	ASSERT_EQUALS(wxString("/tmp/remote.conf"), FinalizeFilename("/tmp/remote.conf"));
}

TEST(ECFileConfig, HomeDirectoryShorthandIsExpanded)
{
	ASSERT_EQUALS(wxGetHomeDir() + "/remote.conf", FinalizeFilename("~/remote.conf"));
	ASSERT_EQUALS(wxGetHomeDir() + "/", FinalizeFilename("~/"));
	ASSERT_EQUALS(wxString("~user/remote.conf"), FinalizeFilename("~user/remote.conf"));
}

#ifdef __WINDOWS__
TEST(ECFileConfig, WindowsPathsArePreserved)
{
	for (const wxString &path : { wxString("C:/tmp/amule.conf"),
		     wxString("C:\\tmp\\amule.conf"),
		     wxString("C:/tmp\\amule.conf"),
		     wxString("C:/amule.conf"),
		     wxString("C:amule.conf"),
		     wxString("\\amule.conf"),
		     wxString(".\\amule.conf"),
		     wxString("..\\amule.conf"),
		     wxString("subdir\\amule.conf"),
		     wxString("\\\\server\\share\\amule.conf"),
		     wxString("//server/share/amule.conf") }) {
		ASSERT_EQUALS(path, FinalizeFilename(path));
	}
}

TEST(ECFileConfig, WindowsHomeDirectoryShorthandIsExpanded)
{
	ASSERT_EQUALS(wxGetHomeDir() + "\\remote.conf", FinalizeFilename("~\\remote.conf"));
	ASSERT_EQUALS(wxGetHomeDir() + "\\", FinalizeFilename("~\\"));
}

TEST(ECFileConfig, WindowsBareStreamNamesUseConfigDirectory)
{
	for (const wxString &name :
		{ wxString("remote.conf:stream"), wxString("remote.conf:stream:$DATA") }) {
		ASSERT_EQUALS(GetConfigDir(name) + name, FinalizeFilename(name));
	}
}
#else
TEST(ECFileConfig, UnixBackslashesAndColonsRemainFilenameCharacters)
{
	for (const wxString &name :
		{ wxString("file\\name.conf"), wxString("~\\remote.conf"), wxString("C:remote.conf") }) {
		ASSERT_EQUALS(GetConfigDir(name) + name, FinalizeFilename(name));
	}
}
#endif
