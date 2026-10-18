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

#include <muleunit/test.h>
#include <wx/intl.h>

#include "Constants.h"
#include <ec/cpp/ECSpecialTags.h>

using namespace muleunit;

// These constructors normally live in ECSpecialCoreTags.cpp and require live
// core files. Supply only the tag envelope here so the production formatter
// can be exercised with synthetic EC data, without the daemon or unsafe casts.
CEC_SharedFile_Tag::CEC_SharedFile_Tag(const CKnownFile *, EC_DETAIL_LEVEL, CValueMap *, ec_tagname_t name)
: CECTag(name, uint32(1))
{
}

CEC_PartFile_Tag::CEC_PartFile_Tag(const CPartFile *, EC_DETAIL_LEVEL detail, CValueMap *valuemap)
: CEC_SharedFile_Tag(nullptr, detail, valuemap, EC_TAG_PARTFILE)
{
}

static wxString StatusText(uint8 status, uint16 sources = 0, int stopped = 0)
{
	CEC_PartFile_Tag tag(nullptr, EC_DETAIL_FULL);
	tag.AddTag(CECTag(EC_TAG_PARTFILE_STATUS, status));
	tag.AddTag(CECTag(EC_TAG_PARTFILE_SOURCE_COUNT_XFER, sources));
	if (stopped >= 0) {
		tag.AddTag(CECTag(EC_TAG_PARTFILE_STOPPED, stopped != 0));
	}
	return tag.GetFileStatusString();
}

DECLARE_SIMPLE(ECPartFileStatus)

TEST(ECPartFileStatus, ExplicitStatesOverrideTransferCount)
{
	const struct
	{
		uint8 status;
		const char *text;
	} cases[] = {
		{ PS_HASHING, "Hashing" },
		{ PS_WAITING_FOR_HASH, "Hashing" },
		{ PS_ALLOCATING, "Allocating" },
		{ PS_COMPLETING, "Completing" },
		{ PS_COMPLETE, "Complete" },
		{ PS_PAUSED, "Paused" },
		{ PS_ERROR, "Erroneous" },
		{ PS_INSUFFICIENT, "Insufficient disk space" },
	};
	for (const auto &entry : cases) {
		for (uint16 sources : { 0, 2 }) {
			ASSERT_EQUALS(wxGetTranslation(entry.text), StatusText(entry.status, sources));
		}
	}
}

TEST(ECPartFileStatus, ActiveAndUnknownStatesUseTransferCount)
{
	for (uint8 status : { PS_READY, PS_EMPTY, PS_UNKNOWN }) {
		ASSERT_EQUALS(_("Waiting"), StatusText(status));
		ASSERT_EQUALS(_("Downloading"), StatusText(status, 2));
	}
}

TEST(ECPartFileStatus, StoppedOverridesOrdinaryStates)
{
	for (uint8 status :
		{ PS_READY, PS_EMPTY, PS_PAUSED, PS_ERROR, PS_INSUFFICIENT, PS_COMPLETING, PS_UNKNOWN }) {
		for (uint16 sources : { 0, 2 }) {
			ASSERT_EQUALS(_("Stopped"), StatusText(status, sources, 1));
		}
	}
}

TEST(ECPartFileStatus, CompleteOverridesStopped)
{
	ASSERT_EQUALS(_("Complete"), StatusText(PS_COMPLETE, 0, 1));
}

TEST(ECPartFileStatus, HashingAndAllocationOverrideStopped)
{
	ASSERT_EQUALS(_("Hashing"), StatusText(PS_HASHING, 0, 1));
	ASSERT_EQUALS(_("Hashing"), StatusText(PS_WAITING_FOR_HASH, 0, 1));
	ASSERT_EQUALS(_("Allocating"), StatusText(PS_ALLOCATING, 0, 1));
}

TEST(ECPartFileStatus, MissingStoppedTagDefaultsToFalse)
{
	ASSERT_EQUALS(_("Paused"), StatusText(PS_PAUSED, 0, -1));
	ASSERT_EQUALS(_("Insufficient disk space"), StatusText(PS_INSUFFICIENT, 0, -1));
	ASSERT_EQUALS(_("Waiting"), StatusText(PS_READY, 0, -1));
	ASSERT_EQUALS(_("Downloading"), StatusText(PS_READY, 2, -1));
}
