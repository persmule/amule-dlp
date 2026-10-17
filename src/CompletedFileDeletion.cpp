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

#include "CompletedFileDeletion.h"

#include <common/Path.h>
#include <common/StringFunctions.h>
#include <filesystem>
#include <system_error>

bool CompletedFilePathIsMissing(const CPath &path)
{
#ifdef __WINDOWS__
	const std::filesystem::path nativePath(path.GetRaw().ToStdWstring());
#else
	const auto nativeName = filename2char(path.GetRaw());
	if (!nativeName.data() || nativeName.length() == 0) {
		return false;
	}
	const std::filesystem::path nativePath(nativeName.data());
#endif
	std::error_code error;
	const auto status = std::filesystem::symlink_status(nativePath, error);
	return status.type() == std::filesystem::file_type::not_found &&
	       (!error || error == std::errc::no_such_file_or_directory);
}
