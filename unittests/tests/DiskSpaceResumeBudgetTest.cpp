// This file is part of the aMule Project.
// Copyright (c) 2003-2026 aMule Team ( https://amule-org.github.io )
// Licensed under the GNU General Public License, version 2 or later.

#include <muleunit/test.h>
#include "DiskSpaceResumeBudget.h"
#include <limits>

using namespace muleunit;

DECLARE_SIMPLE(DiskSpaceResumeBudget)

TEST(DiskSpaceResumeBudget, NeededSpaceLargerThanFreeDoesNotWrap)
{
	CDiskSpaceResumeBudget budget(100, 10);
	ASSERT_FALSE(budget.Fits(101));
	ASSERT_TRUE(budget.Fits(89));
}

TEST(DiskSpaceResumeBudget, MultipleFilesCannotSpendTheSameFreeSpace)
{
	CDiskSpaceResumeBudget budget(100, 10);
	ASSERT_TRUE(budget.Fits(60));
	budget.Charge(60);
	ASSERT_FALSE(budget.Fits(60));
	ASSERT_TRUE(budget.Fits(29));
	budget.Charge(29);
	ASSERT_FALSE(budget.Fits(1));
}

TEST(DiskSpaceResumeBudget, FailedResumeLeavesSpaceForTheNextFile)
{
	CDiskSpaceResumeBudget budget(100, 10);
	// A failed ResumeFile does not call Charge.
	ASSERT_TRUE(budget.Fits(80));
	ASSERT_TRUE(budget.Fits(89));
}

TEST(DiskSpaceResumeBudget, PreservesStrictMinimumFreeSpaceBoundary)
{
	for (uint64_t free : { 0u, 9u, 10u }) {
		CDiskSpaceResumeBudget budget(free, 10);
		ASSERT_FALSE(budget.Fits(0));
	}
	CDiskSpaceResumeBudget budget(100, 10);
	ASSERT_FALSE(budget.Fits(90));
	ASSERT_TRUE(budget.Fits(89));
	ASSERT_TRUE(budget.Fits(0));
}

TEST(DiskSpaceResumeBudget, MaximumValuesCannotOverflowThreshold)
{
	const uint64_t maximum = std::numeric_limits<uint64_t>::max();
	CDiskSpaceResumeBudget budget(maximum, maximum - 10);
	ASSERT_FALSE(budget.Fits(maximum));
	ASSERT_FALSE(budget.Fits(10));
	ASSERT_TRUE(budget.Fits(9));
}

TEST(DiskSpaceResumeBudget, ActiveDownloadsCanExhaustTheBudget)
{
	for (uint64_t needed : { uint64_t(90), uint64_t(91), std::numeric_limits<uint64_t>::max() }) {
		CDiskSpaceResumeBudget budget(100, 10);
		budget.Charge(needed);
		ASSERT_FALSE(budget.Fits(0));
		ASSERT_FALSE(budget.Fits(1));
		// Further active downloads must not wrap an exhausted budget.
		budget.Charge(1);
		ASSERT_FALSE(budget.Fits(0));
	}
}

TEST(DiskSpaceResumeBudget, ActiveDownloadsRemainReservedAcrossChecks)
{
	const uint64_t fileSize = uint64_t(3) * 1024 * 1024 * 1024;
	const uint64_t minimum = 100 * 1024 * 1024;
	const uint64_t free = minimum + uint64_t(4500) * 1024 * 1024;
	unsigned active = 0;
	for (unsigned check = 0; check < 3; ++check) {
		CDiskSpaceResumeBudget budget(free, minimum);
		for (unsigned file = 0; file < active; ++file) {
			budget.Charge(fileSize);
		}
		for (unsigned file = active; file < 3; ++file) {
			if (budget.Fits(fileSize)) {
				budget.Charge(fileSize);
				++active;
			}
		}
		ASSERT_EQUALS(1u, active);
	}
}
