// This file is part of the aMule Project.
// Copyright (c) 2003-2026 aMule Team ( https://amule-org.github.io )
// Licensed under the GNU General Public License, version 2 or later.

#include <muleunit/test.h>
#include "KadPublishCursor.h"

#include <map>

using namespace muleunit;

DECLARE_SIMPLE(KadPublishCursor)

TEST(KadPublishCursor, EmptyAndSingleFile)
{
	std::map<int, int> files;
	CKadPublishCursor<int> cursor;
	ASSERT_TRUE(cursor.Next(files) == files.end());
	files.emplace(0, 0);
	for (int i = 0; i < 3; ++i) {
		ASSERT_EQUALS(0, cursor.Next(files)->first);
	}
}

TEST(KadPublishCursor, WrapsWithoutAnEmptyTick)
{
	const std::map<int, int> files{ { 0, 0 }, { 10, 0 }, { 20, 0 } };
	CKadPublishCursor<int> cursor;
	for (int pass = 0; pass < 2; ++pass) {
		ASSERT_EQUALS(0, cursor.Next(files)->first);
		ASSERT_EQUALS(10, cursor.Next(files)->first);
		ASSERT_EQUALS(20, cursor.Next(files)->first);
	}
}

TEST(KadPublishCursor, AddAndRemoveDuringPass)
{
	std::map<int, int> files{ { 10, 0 }, { 20, 0 }, { 30, 0 } };
	CKadPublishCursor<int> cursor;
	ASSERT_EQUALS(10, cursor.Next(files)->first);
	files.erase(10); // Removing the last selected file must not invalidate the cursor.
	files.erase(20); // Nor may removal of the next file skip its successor.
	files.emplace(5, 0);
	files.emplace(25, 0);
	ASSERT_EQUALS(25, cursor.Next(files)->first);
	ASSERT_EQUALS(30, cursor.Next(files)->first);
	ASSERT_EQUALS(5, cursor.Next(files)->first);
}

TEST(KadPublishCursor, ClearAndReload)
{
	std::map<int, int> files{ { 10, 0 } };
	CKadPublishCursor<int> cursor;
	ASSERT_EQUALS(10, cursor.Next(files)->first);
	files.clear();
	ASSERT_TRUE(cursor.Next(files) == files.end());
	files.emplace(5, 0);
	ASSERT_EQUALS(5, cursor.Next(files)->first);
}

TEST(KadPublishCursor, SourceAndNotesAdvanceIndependently)
{
	const std::map<int, int> files{ { 10, 0 }, { 20, 0 } };
	CKadPublishCursor<int> source;
	CKadPublishCursor<int> notes;
	ASSERT_EQUALS(10, source.Next(files)->first);
	ASSERT_EQUALS(20, source.Next(files)->first);
	ASSERT_EQUALS(10, notes.Next(files)->first);
	ASSERT_EQUALS(10, source.Next(files)->first);
	ASSERT_EQUALS(20, notes.Next(files)->first);
}

TEST(KadPublishCursor, SixtyThousandFiles)
{
	std::map<int, int> files;
	for (int i = 0; i < 60000; ++i) {
		files.emplace(i, 0);
	}
	CKadPublishCursor<int> cursor;
	for (int pass = 0; pass < 2; ++pass) {
		for (int i = 0; i < 60000; ++i) {
			ASSERT_EQUALS(i, cursor.Next(files)->first);
		}
	}
}
