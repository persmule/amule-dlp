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

#include "DisplayRateCounter.h"
#include <muleunit/test.h>

#include <thread>
#include <vector>

using namespace muleunit;

namespace
{
uint64_t RateAt(CDisplayRateCounter &counter, uint64_t now)
{
	counter.CalculateRate(now);
	return counter.GetRate();
}
} // namespace

DECLARE_SIMPLE(DisplayRateCounterTest)

TEST(DisplayRateCounterTest, StartupIncludesIdleTime)
{
	CDisplayRateCounter rate;
	ASSERT_EQUALS(uint64_t(0), RateAt(rate, 0));
	// 100 KiB delivered in 100 ms must not be presented as a 1000 KiB/s spike.
	rate.Add(100 * 1024, 100);
	ASSERT_EQUALS(uint64_t(100 * 1024), RateAt(rate, 100));
}

TEST(DisplayRateCounterTest, SteadyTransferAndStop)
{
	CDisplayRateCounter rate;
	for (uint64_t now = 100; now <= 3000; now += 100) {
		rate.Add(30 * 1024, now);
		if (now >= 1000) {
			ASSERT_EQUALS(uint64_t(300 * 1024), RateAt(rate, now));
		}
	}
	ASSERT_EQUALS(uint64_t(150 * 1024), RateAt(rate, 3500));
	// No new Add call: the periodic calculation must still expire a stopped transfer.
	ASSERT_EQUALS(uint64_t(0), RateAt(rate, 4000));
	ASSERT_EQUALS(uint64_t(0), RateAt(rate, 100000));
}

TEST(DisplayRateCounterTest, SpeedChangeReplacesOldWindow)
{
	CDisplayRateCounter rate;
	for (uint64_t now = 100; now <= 1000; now += 100) {
		rate.Add(10 * 1024, now);
	}
	for (uint64_t now = 1100; now <= 2000; now += 100) {
		rate.Add(40 * 1024, now);
	}
	ASSERT_EQUALS(uint64_t(400 * 1024), RateAt(rate, 2000));
}

TEST(DisplayRateCounterTest, PeerTotalsDifferOnlyByControlTraffic)
{
	CDisplayRateCounter total, first, second;
	for (uint64_t now = 100; now <= 2000; now += 100) {
		first.Add(2000, now);
		second.Add(3000, now);
		total.Add(5000, now);
		total.Add(100, now); // control packets in the same millisecond
	}
	ASSERT_EQUALS(RateAt(first, 2000) + RateAt(second, 2000) + 1000, RateAt(total, 2000));
	ASSERT_EQUALS(uint64_t(0), RateAt(total, 3000));
}

TEST(DisplayRateCounterTest, NewUploadSessionStartsEmpty)
{
	CDisplayRateCounter rate;
	rate.Add(10000, 500);
	ASSERT_EQUALS(uint64_t(10000), RateAt(rate, 500));
	rate.Reset();
	ASSERT_EQUALS(uint64_t(0), rate.GetRate());
	ASSERT_EQUALS(uint64_t(0), RateAt(rate, 500));
	rate.Add(500, 600);
	ASSERT_EQUALS(uint64_t(500), RateAt(rate, 600));
}

TEST(DisplayRateCounterTest, WideTotalsAndReorderedProducers)
{
	CDisplayRateCounter rate;
	const uint64_t bytes = uint64_t(1) << 32;
	rate.Add(bytes, 500);
	rate.Add(bytes, 499); // a producer was delayed before acquiring the lock
	ASSERT_EQUALS(bytes * 2, RateAt(rate, 500));
	ASSERT_EQUALS(bytes, RateAt(rate, 1499));
	ASSERT_EQUALS(uint64_t(0), RateAt(rate, 1500));
}

TEST(DisplayRateCounterTest, ReadsStayStableUntilPublication)
{
	CDisplayRateCounter rate;
	rate.Add(1200, 100);
	ASSERT_EQUALS(uint64_t(0), rate.GetRate());
	rate.CalculateRate(100);
	rate.Add(800, 200);
	ASSERT_EQUALS(uint64_t(1200), rate.GetRate());
	rate.CalculateRate(1100);
	ASSERT_EQUALS(uint64_t(800), rate.GetRate());
	rate.CalculateRate(1200);
	ASSERT_EQUALS(uint64_t(0), rate.GetRate());
}

TEST(DisplayRateCounterTest, LateProducerCannotResurrectExpiredBytes)
{
	CDisplayRateCounter rate;
	rate.Add(1000, 100);
	ASSERT_EQUALS(uint64_t(0), RateAt(rate, 2000));
	rate.Add(2000, 999); // producer resumed after the reader expired the whole window
	ASSERT_EQUALS(uint64_t(0), RateAt(rate, 2000));
	rate.Add(3000, 1500); // late, but still inside the current window
	ASSERT_EQUALS(uint64_t(3000), RateAt(rate, 2000));
	ASSERT_EQUALS(uint64_t(0), RateAt(rate, 2500));
}

TEST(DisplayRateCounterTest, OlderPublisherCannotRewindWindow)
{
	CDisplayRateCounter rate;
	rate.Add(1000, 100);
	rate.CalculateRate(2000);
	rate.CalculateRate(100);
	rate.Add(2000, 100);
	ASSERT_EQUALS(uint64_t(0), RateAt(rate, 2000));
}

TEST(DisplayRateCounterTest, WireRatesSaturateInsteadOfWrapping)
{
	ASSERT_EQUALS(uint32_t(1234), CDisplayRateCounter::ClampRate(1234));
	const uint64_t limit = (uint64_t(1) << 32) - 1;
	ASSERT_EQUALS(uint32_t(limit), CDisplayRateCounter::ClampRate(limit));
	ASSERT_EQUALS(uint32_t(limit), CDisplayRateCounter::ClampRate(limit + 1));
	ASSERT_EQUALS(uint32_t(limit), CDisplayRateCounter::ClampRate(uint64_t(1) << 40));
}

TEST(DisplayRateCounterTest, ConcurrentProducersKeepEveryByte)
{
	CDisplayRateCounter rate;
	std::vector<std::thread> threads;
	for (unsigned i = 0; i < 4; ++i) {
		threads.emplace_back([&rate, i]() {
			for (unsigned j = 0; j < 1000; ++j) {
				rate.Add(123, (uint64_t(1) << 40) + i);
			}
		});
	}
	for (auto &thread : threads) {
		thread.join();
	}
	ASSERT_EQUALS(uint64_t(4 * 1000 * 123), RateAt(rate, (uint64_t(1) << 40) + 3));
	ASSERT_EQUALS(uint64_t(0), RateAt(rate, (uint64_t(1) << 40) + 1003));
}

TEST(DisplayRateCounterTest, DaemonBatchesCoverTheirFullIntervals)
{
	CDisplayRateCounter rate;
	for (uint64_t now = 300; now <= 6000; now += 300) {
		rate.AddBatch(240 * 1024, now - 300, now);
		const uint64_t expected = now < 1000 ? now * 800 * 1024 / 1000 : 800 * 1024;
		ASSERT_EQUALS(expected, RateAt(rate, now));
	}
	ASSERT_EQUALS(uint64_t(0), RateAt(rate, 7000));
}

TEST(DisplayRateCounterTest, BatchedSpeedChangeAndIdleDoNotInflateRate)
{
	CDisplayRateCounter rate;
	for (uint64_t now = 300; now <= 1200; now += 300) {
		rate.AddBatch(240000, now - 300, now);
	}
	for (uint64_t now = 1500; now <= 2400; now += 300) {
		rate.AddBatch(120000, now - 300, now);
	}
	ASSERT_EQUALS(uint64_t(400000), RateAt(rate, 2400));
	rate.AddBatch(0, 2400, 2700);
	ASSERT_EQUALS(uint64_t(300000), RateAt(rate, 2700));
	ASSERT_EQUALS(uint64_t(0), RateAt(rate, 3400));
	// After idle, only the actual last tick belongs to a new batch.
	rate.AddBatch(240000, 5700, 6000);
	ASSERT_EQUALS(uint64_t(240000), RateAt(rate, 6000));
}

TEST(DisplayRateCounterTest, GuiBatchesMatchPacketWindow)
{
	CDisplayRateCounter packets, batches;
	for (uint64_t now = 100; now <= 3000; now += 100) {
		packets.Add(80000, now);
		batches.AddBatch(80000, now - 100, now);
		ASSERT_EQUALS(RateAt(packets, now), RateAt(batches, now));
	}
}

TEST(DisplayRateCounterTest, DelayedBatchUsesActualElapsedTime)
{
	CDisplayRateCounter rate;
	rate.AddBatch(4000000, 1000, 6000);
	ASSERT_EQUALS(uint64_t(800000), RateAt(rate, 6000));
	ASSERT_EQUALS(uint64_t(0), RateAt(rate, 7000));
	rate.Reset();
	rate.AddBatch(80000, 8000, 8100);
	ASSERT_EQUALS(uint64_t(80000), RateAt(rate, 8100));
}
