// This file is part of the aMule Project.
// Copyright (c) 2003-2026 aMule Team ( https://amule-org.github.io )
// Licensed under the GNU General Public License, version 2 or later.

#include <muleunit/test.h>
#include "ServerUDPPacketQueue.h"
#include <protocol/Protocols.h>
#include <type_traits>
#include <array>

using namespace muleunit;

static_assert(!std::is_copy_constructible_v<CServerUDPPacketQueue>);
static_assert(!std::is_copy_assignable_v<CServerUDPPacketQueue>);
static_assert(!std::is_move_constructible_v<CServerUDPPacketQueue>);
static_assert(!std::is_move_assignable_v<CServerUDPPacketQueue>);

namespace
{
ServerUDPPacket Pending(uint32 bytes, uint32 ip = 0)
{
	ServerUDPPacket item;
	item.packet = std::make_unique<CPacket>(int8{ 0 }, bytes, OP_EDONKEYPROT);
	item.ip = ip;
	item.port = 4665;
	if (!ip) {
		item.addr = "unresolved.example";
	}
	return item;
}
} // namespace

DECLARE_SIMPLE(ServerUDPPacketQueue)

TEST(ServerUDPPacketQueue, StalledDnsCannotRetainUnlimitedEmptyPackets)
{
	CServerUDPPacketQueue queue;
	for (size_t i = 0; i < CServerUDPPacketQueue::MaxPackets; ++i) {
		ASSERT_TRUE(queue.Push(Pending(0)));
	}
	CPacket *first = queue.Front().packet.get();
	ASSERT_FALSE(queue.Push(Pending(0)));
	ASSERT_TRUE(queue.Front().packet.get() == first);
	ASSERT_TRUE(queue.Front().addr == "unresolved.example");
	ASSERT_TRUE(queue.Size() == CServerUDPPacketQueue::MaxPackets);
	ASSERT_TRUE(queue.Bytes() == 2 * CServerUDPPacketQueue::MaxPackets);

	queue.Pop();
	ASSERT_TRUE(queue.Push(Pending(0)));
}

TEST(ServerUDPPacketQueue, WireByteBudgetIncludesHeadersAndRecoversAfterDrop)
{
	CServerUDPPacketQueue queue;
	constexpr uint32 wireBytes = 32768;
	for (size_t i = 0; i < CServerUDPPacketQueue::MaxBytes / wireBytes; ++i) {
		ASSERT_TRUE(queue.Push(Pending(wireBytes - 2)));
	}
	ASSERT_TRUE(queue.Bytes() == CServerUDPPacketQueue::MaxBytes);
	CPacket *first = queue.Front().packet.get();
	ASSERT_FALSE(queue.Push(Pending(0)));
	ASSERT_TRUE(queue.Front().packet.get() == first);

	queue.Pop();
	ASSERT_TRUE(queue.Bytes() == CServerUDPPacketQueue::MaxBytes - wireBytes);
	ASSERT_TRUE(queue.Push(Pending(wireBytes - 2)));
	ASSERT_FALSE(queue.Push(Pending(0)));
}

TEST(ServerUDPPacketQueue, OversizedPacketCannotEnterAnEmptyQueue)
{
	CServerUDPPacketQueue queue;
	ASSERT_FALSE(queue.Push(Pending(CServerUDPPacketQueue::MaxBytes - 1)));
	ASSERT_TRUE(queue.Empty());
	ASSERT_TRUE(queue.Bytes() == 0);
	ASSERT_TRUE(queue.Push(Pending(0)));
}

TEST(ServerUDPPacketQueue, DnsRotationPreservesOwnershipOrderAndBudget)
{
	CServerUDPPacketQueue queue;
	auto unresolved = Pending(100);
	CPacket *first = unresolved.packet.get();
	ASSERT_TRUE(queue.Push(std::move(unresolved)));
	ASSERT_TRUE(queue.Push(Pending(200, 0x01020304)));
	queue.RotateFront();
	ASSERT_TRUE(queue.Size() == 2);
	ASSERT_TRUE(queue.Bytes() == 304);
	ASSERT_TRUE(queue.Front().ip == 0x01020304);

	// Delivery transfers the same owned packet to the sending socket.
	auto outgoing = queue.Pop();
	ASSERT_TRUE(outgoing.packet != nullptr);
	ASSERT_TRUE(outgoing.port == 4665);
	ASSERT_TRUE(queue.Bytes() == 102);
	ASSERT_TRUE(queue.Front().packet.get() == first);
	queue.Pop();
	ASSERT_TRUE(queue.Empty());
	ASSERT_TRUE(queue.Bytes() == 0);
}

TEST(ServerUDPPacketQueue, RejectsMissingPacketWithoutConsumingBudget)
{
	CServerUDPPacketQueue queue;
	ASSERT_FALSE(queue.Push(ServerUDPPacket{}));
	ASSERT_TRUE(queue.Empty());
	ASSERT_TRUE(queue.Bytes() == 0);
}

TEST(ServerUDPPacketQueue, RepeatedFailureAndDeliveryReleaseTheEntireBudget)
{
	CServerUDPPacketQueue queue;
	for (unsigned cycle = 0; cycle < 3; ++cycle) {
		// Fill with mixed packet sizes, then use the remaining budget exactly.
		ASSERT_TRUE(queue.Push(Pending(30000, 0x01020304)));
		ASSERT_TRUE(queue.Push(Pending(123)));
		const uint32 remainingPayload = CServerUDPPacketQueue::MaxBytes - queue.Bytes() - 2;
		ASSERT_TRUE(queue.Push(Pending(remainingPayload)));
		ASSERT_FALSE(queue.Push(Pending(0)));

		queue.RotateFront();
		queue.Pop(); // failed DNS: delete the packet rather than deliver it
		ASSERT_TRUE(queue.Bytes() == CServerUDPPacketQueue::MaxBytes - 125);
		auto failed = queue.Pop();
		ASSERT_TRUE(failed.packet->GetPacketSize() == remainingPayload);
		ASSERT_TRUE(queue.Bytes() == 30002);
		auto delivered = queue.Pop();
		ASSERT_TRUE(delivered.ip == 0x01020304);
		// The socket takes ownership after removal from the DNS queue.
		std::unique_ptr<CPacket> socketPacket(delivered.packet.release());
		ASSERT_TRUE(socketPacket->GetPacketSize() == 30000);
		ASSERT_TRUE(queue.Empty());
		ASSERT_TRUE(queue.Bytes() == 0);
	}
}

TEST(ServerUDPPacketQueue, RawDatagramBytesSurviveQueueingAndOwnershipTransfer)
{
	// Obfuscated server packets carry their first two bytes as protocol/opcode.
	// They must pass through the DNS queue without being repacked or modified.
	const std::array<uint8_t, 5> payload = { 0x01, 0x00, 0xfe, 0xff, 0x42 };
	ServerUDPPacket raw;
	raw.packet = std::make_unique<CPacket>(int8{ 0x53 }, payload.size(), uint8{ 0xa7 });
	raw.packet->CopyToDataBuffer(0, payload.data(), payload.size());
	raw.ip = 0x01020304;
	raw.port = 4672;
	CPacket *original = raw.packet.get();
	CServerUDPPacketQueue queue;
	ASSERT_TRUE(queue.Push(std::move(raw)));
	ASSERT_TRUE(queue.Push(Pending(10)));
	queue.RotateFront();
	queue.Pop();
	auto outgoing = queue.Pop();
	std::unique_ptr<CPacket> delivered(outgoing.packet.release());
	ASSERT_TRUE(delivered.get() == original);
	ASSERT_TRUE(outgoing.ip == 0x01020304);
	ASSERT_TRUE(outgoing.port == 4672);
	ASSERT_TRUE(delivered->GetUDPHeader()[0] == 0xa7);
	ASSERT_TRUE(delivered->GetUDPHeader()[1] == 0x53);
	ASSERT_TRUE(delivered->GetPacketSize() == payload.size());
	for (size_t i = 0; i < payload.size(); ++i) {
		ASSERT_TRUE(delivered->GetDataBuffer()[i] == payload[i]);
	}
	ASSERT_TRUE(queue.Bytes() == 0);
}
