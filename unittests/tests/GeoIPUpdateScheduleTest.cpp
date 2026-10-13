// This file is part of the aMule Project.
// Copyright (c) 2003-2026 aMule Team ( https://amule-org.github.io )
// Licensed under the GNU General Public License, version 2 or later.

#include <muleunit/test.h>
#include "geoip/UpdateSchedule.h"

using namespace muleunit;
DECLARE_SIMPLE(GeoIPUpdateSchedule)

TEST(GeoIPUpdateSchedule, WaitsOneWeekFromStartup)
{
	CGeoIPUpdateSchedule schedule(12345);
	ASSERT_FALSE(schedule.IsDue(12345, true, true, false));
	ASSERT_FALSE(schedule.IsDue(12345 + CGeoIPUpdateSchedule::Interval - 1, true, true, false));
	ASSERT_TRUE(schedule.IsDue(12345 + CGeoIPUpdateSchedule::Interval, true, true, false));
}

TEST(GeoIPUpdateSchedule, DisabledPreferencesSuppressUpdates)
{
	CGeoIPUpdateSchedule schedule;
	ASSERT_FALSE(schedule.IsDue(CGeoIPUpdateSchedule::Interval, false, true, false));
	ASSERT_FALSE(schedule.IsDue(CGeoIPUpdateSchedule::Interval, true, false, false));
	ASSERT_TRUE(schedule.IsDue(CGeoIPUpdateSchedule::Interval, true, true, false));
}

TEST(GeoIPUpdateSchedule, InFlightDownloadDefersUpdate)
{
	CGeoIPUpdateSchedule schedule;
	ASSERT_FALSE(schedule.IsDue(CGeoIPUpdateSchedule::Interval, true, true, true));
	ASSERT_TRUE(schedule.IsDue(CGeoIPUpdateSchedule::Interval + 1000, true, true, false));
}

TEST(GeoIPUpdateSchedule, AttemptsResetIntervalIncludingFailuresAndManualUpdates)
{
	CGeoIPUpdateSchedule schedule;
	const uint64_t attempt = CGeoIPUpdateSchedule::Interval + 1000;
	schedule.RecordAttempt(attempt);
	ASSERT_FALSE(schedule.IsDue(attempt + CGeoIPUpdateSchedule::Interval - 1, true, true, false));
	ASSERT_TRUE(schedule.IsDue(attempt + CGeoIPUpdateSchedule::Interval, true, true, false));
}

TEST(GeoIPUpdateSchedule, ClockJumpBackProducesOneAttemptThenWaits)
{
	CGeoIPUpdateSchedule schedule(CGeoIPUpdateSchedule::Interval);
	ASSERT_TRUE(schedule.IsDue(1000, true, true, false));
	schedule.RecordAttempt(1000);
	ASSERT_FALSE(schedule.IsDue(1000 + CGeoIPUpdateSchedule::Interval - 1, true, true, false));
}

TEST(GeoIPUpdateSchedule, LongGapProducesOneAttemptThenWaits)
{
	CGeoIPUpdateSchedule schedule;
	const uint64_t resumed = 100 * CGeoIPUpdateSchedule::Interval;
	ASSERT_TRUE(schedule.IsDue(resumed, true, true, false));
	schedule.RecordAttempt(resumed);
	ASSERT_FALSE(schedule.IsDue(resumed + 1000, true, true, false));
}
