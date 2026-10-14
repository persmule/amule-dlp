// Copyright (c) 2026 aMule Team
// SPDX-License-Identifier: GPL-2.0-or-later
#include <muleunit/test.h>
#include "UploadRefill.h"

using namespace muleunit;
DECLARE_SIMPLE(UploadRefill)

namespace
{
constexpr uint32_t block = 184320;
}

TEST(UploadRefill, SlowSlotRefillsWithABlockInReserve)
{
	UploadRefill refill;
	refill.SetTarget(block + 1);
	refill.Queue(2 * block);
	ASSERT_FALSE(refill.Complete(10240));
	ASSERT_TRUE(refill.Complete(block - 10240));
	// A delayed read must not cause a wake-up on every following packet.
	ASSERT_FALSE(refill.Complete(10240));
	refill.Queue(block);
	ASSERT_TRUE(refill.Complete(block - 10240));
}

TEST(UploadRefill, FastSlotKeepsItsSendAheadReserve)
{
	UploadRefill refill;
	refill.SetTarget(10 * block + 1);
	refill.Queue(11 * block);
	ASSERT_FALSE(refill.Complete(block - 1));
	ASSERT_TRUE(refill.Complete(1));
	refill.Queue(block);
	ASSERT_TRUE(refill.Complete(block));
}

TEST(UploadRefill, ShortRequestAndCompressedPacketDrainWakeTheWorker)
{
	UploadRefill refill;
	refill.SetTarget(block + 1);
	refill.Queue(4096);
	ASSERT_TRUE(refill.Complete(4096));
	// Completion uses original payload, irrespective of compressed wire length.
	refill.Queue(2 * block);
	ASSERT_TRUE(refill.Complete(block));
	ASSERT_TRUE(refill.Complete(block));
}

TEST(UploadRefill, NoPayloadAndInactiveSocketsDoNotWake)
{
	UploadRefill refill;
	refill.Queue(block);
	ASSERT_FALSE(refill.Complete(block));
	refill.SetTarget(block + 1);
	ASSERT_FALSE(refill.Complete(0));
}

TEST(UploadRefill, DisconnectClearsOldPayloadAndTarget)
{
	UploadRefill refill;
	refill.SetTarget(block + 1);
	refill.Queue(2 * block);
	refill.Clear();
	refill.Queue(4096);
	ASSERT_FALSE(refill.Complete(4096));
	refill.SetTarget(block + 1);
	refill.Queue(4096);
	ASSERT_TRUE(refill.Complete(4096));
}

TEST(UploadRefill, TruncationKeepsTheCurrentPacketInTheCount)
{
	UploadRefill refill;
	refill.SetTarget(block + 1);
	refill.Queue(2 * block);
	refill.Discard(block);
	ASSERT_TRUE(refill.Complete(block));
}

TEST(UploadRefill, RaisingTargetAfterDrainRequestsAnotherWorkerPass)
{
	UploadRefill refill;
	refill.SetTarget(block + 1);
	refill.Queue(11 * block);
	// The worker's fast-slot snapshot still sees eleven blocks. A completion then
	// drains two blocks while the old slow-slot target is installed: no crossing.
	ASSERT_FALSE(refill.Complete(2 * block));
	ASSERT_TRUE(refill.SetTarget(10 * block + 1));
	// Coalesce repeated checks while the refill is in progress.
	ASSERT_FALSE(refill.SetTarget(10 * block + 1));
	refill.Queue(2 * block);
	ASSERT_TRUE(refill.Complete(block));
}

TEST(UploadRefill, RaisingTargetWithEnoughQueuedPayloadWaitsForCrossing)
{
	UploadRefill refill;
	refill.SetTarget(block + 1);
	refill.Queue(11 * block);
	ASSERT_FALSE(refill.SetTarget(10 * block + 1));
	ASSERT_TRUE(refill.Complete(block));
}

TEST(UploadRefill, LoweringTargetDoesNotScheduleAnUnneededRefill)
{
	UploadRefill refill;
	refill.SetTarget(10 * block + 1);
	refill.Queue(2 * block);
	ASSERT_FALSE(refill.SetTarget(block + 1));
	ASSERT_TRUE(refill.Complete(block));
}
