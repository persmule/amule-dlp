// This file is part of the aMule Project.
// Copyright (c) 2003-2026 aMule Team ( https://amule-org.github.io )
// Licensed under the GNU General Public License, version 2 or later.

#include <muleunit/test.h>
#include <AmuleApiRestart.h>

using namespace muleunit;

DECLARE_SIMPLE(AmuleApiRestart)

TEST(AmuleApiRestart, FailedChildWaitsBeforeRestart)
{
	AmuleApiRestart restart;
	ASSERT_FALSE(restart.IsDue(100000));
	restart.Started(1000);
	restart.Failed(31000);
	ASSERT_FALSE(restart.IsDue(35999));
	ASSERT_TRUE(restart.IsDue(36000));
	restart.Started(36000);
	ASSERT_FALSE(restart.IsDue(100000));
}

TEST(AmuleApiRestart, CrashLoopAndFailedLaunchBackOffWithCap)
{
	AmuleApiRestart restart;
	uint64_t now = 1000;
	for (uint64_t delay : { 5000, 10000, 20000, 40000, 60000, 60000 }) {
		restart.Started(now);
		restart.Failed(++now);
		ASSERT_EQUALS(delay, restart.DelayMs());
		ASSERT_FALSE(restart.IsDue(now + delay - 1));
		ASSERT_TRUE(restart.IsDue(now + delay));
		now += delay;
	}
	// A launch failure has no Started notification and must retain the cap.
	restart.Failed(now);
	ASSERT_EQUALS(uint64_t(60000), restart.DelayMs());
}

TEST(AmuleApiRestart, StableChildResetsBackoffAtOneMinute)
{
	AmuleApiRestart restart;
	restart.Started(0);
	restart.Failed(1);
	restart.Started(5001);
	restart.Failed(65000);
	ASSERT_EQUALS(uint64_t(10000), restart.DelayMs());
	restart.Started(75000);
	restart.Failed(135000);
	ASSERT_EQUALS(uint64_t(5000), restart.DelayMs());
}

TEST(AmuleApiRestart, CancellationRemovesPendingRetryAndBackoff)
{
	AmuleApiRestart restart;
	restart.Started(0);
	restart.Failed(1);
	restart.Failed(5001);
	restart.Cancel();
	ASSERT_FALSE(restart.IsDue(100000));
	restart.Started(100000);
	restart.Failed(100001);
	ASSERT_EQUALS(uint64_t(5000), restart.DelayMs());
}
