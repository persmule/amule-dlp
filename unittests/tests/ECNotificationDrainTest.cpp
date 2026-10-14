// This file is part of the aMule Project.
// Copyright (c) 2026 aMule Team ( https://amule-org.github.io )
// Licensed under GNU GPL version 2 or later; see COPYING.

#include <muleunit/test.h>
#include "ECNotificationDrain.h"

#include <algorithm>
#include <deque>
#include <functional>
#include <memory>
#include <stdexcept>
#include <utility>
#include <wx/app.h>
#include <wx/evtloop.h>

using namespace muleunit;
DECLARE_SIMPLE(ECNotificationDrain)

namespace
{
using EventQueue = std::deque<std::function<void()>>;

class CLoopExitTimer : public wxTimer
{
public:
	CLoopExitTimer(wxEventLoopBase &loop, int code)
	: loop(loop)
	, code(code)
	{
	}
	void Notify() override { loop.ScheduleExit(code); }

private:
	wxEventLoopBase &loop;
	int code;
};

// Every packet is accepted immediately and generates no writable event: the
// notification chain must resume exclusively through deferred continuations.
class CPushPeer
{
public:
	explicit CPushPeer(EventQueue &events)
	: events(events)
	{
	}

	void Drain()
	{
		drain.Run(
			[this]() {
				if (blocked || remaining == 0) {
					return;
				}
				++depth;
				maxDepth = std::max(maxDepth, depth);
				--remaining;
				++sent;
				if (blockAfter == sent) {
					blocked = true;
				}
				Drain();
				--depth;
			},
			[this](auto continuation) { events.emplace_back(std::move(continuation)); });
	}

	CECNotificationDrain drain;
	EventQueue &events;
	int remaining = 0;
	int sent = 0;
	int depth = 0;
	int maxDepth = 0;
	int blockAfter = -1;
	bool blocked = false;
};

void RunNext(EventQueue &events)
{
	ASSERT_FALSE(events.empty());
	auto callback = std::move(events.front());
	events.pop_front();
	callback();
}
} // namespace

TEST(ECNotificationDrain, AcceptedPacketsResumeAfterCapAndYieldToOtherEvents)
{
	EventQueue events;
	CPushPeer peer(events);
	peer.remaining = 25;
	bool otherEventRan = false;
	events.emplace_back([&otherEventRan]() { otherEventRan = true; });
	peer.Drain();
	ASSERT_EQUALS(8, peer.sent);
	ASSERT_EQUALS(2u, events.size());
	RunNext(events);
	ASSERT_TRUE(otherEventRan);
	ASSERT_EQUALS(8, peer.sent);
	while (!events.empty()) {
		RunNext(events);
	}
	ASSERT_EQUALS(25, peer.sent);
	ASSERT_EQUALS(8, peer.maxDepth);
}

TEST(ECNotificationDrain, MultipleDrainsCoalesceTheirContinuation)
{
	EventQueue events;
	CPushPeer peer(events);
	peer.remaining = 40;
	peer.Drain();
	peer.Drain();
	ASSERT_EQUALS(16, peer.sent);
	ASSERT_EQUALS(1u, events.size());
	while (!events.empty()) {
		RunNext(events);
	}
	ASSERT_EQUALS(40, peer.sent);
}

TEST(ECNotificationDrain, BackpressureWaitsForWritableWithoutPolling)
{
	EventQueue events;
	CPushPeer peer(events);
	peer.remaining = 25;
	peer.blockAfter = 8;
	peer.Drain();
	ASSERT_EQUALS(8, peer.sent);
	RunNext(events);
	ASSERT_TRUE(events.empty());
	ASSERT_EQUALS(8, peer.sent);
	// The next writable event drains output and allows pulling more updates.
	peer.blocked = false;
	peer.Drain();
	while (!events.empty()) {
		RunNext(events);
	}
	ASSERT_EQUALS(25, peer.sent);
}

TEST(ECNotificationDrain, ConnectionLossCancelsPendingAndFutureDrains)
{
	EventQueue events;
	CPushPeer peer(events);
	peer.remaining = 25;
	peer.Drain();
	peer.drain.Cancel();
	RunNext(events);
	peer.Drain();
	ASSERT_EQUALS(8, peer.sent);
	ASSERT_TRUE(events.empty());
}

TEST(ECNotificationDrain, DestroyedOwnerIsNotCalledByQueuedContinuation)
{
	EventQueue events;
	{
		auto peer = std::make_unique<CPushPeer>(events);
		peer->remaining = 25;
		peer->Drain();
		ASSERT_EQUALS(1u, events.size());
	}
	// Both stored functions captured the owner; the weak guard must suppress them.
	RunNext(events);
	ASSERT_TRUE(events.empty());
}

TEST(ECNotificationDrain, EmptyNotifierDoesNotQueueAContinuation)
{
	EventQueue events;
	CPushPeer peer(events);
	peer.Drain();
	ASSERT_TRUE(events.empty());
}

TEST(ECNotificationDrain, CancellationDuringSendKeepsActiveDispatchStateAlive)
{
	CECNotificationDrain drain;
	EventQueue events;
	drain.Run([&drain]() { drain.Cancel(); },
		[&events](auto continuation) { events.emplace_back(std::move(continuation)); });
	ASSERT_TRUE(events.empty());
}

TEST(ECNotificationDrain, FailedSchedulingDoesNotSuppressTheNextContinuation)
{
	CECNotificationDrain drain;
	EventQueue events;
	int sent = 0;
	bool rejectContinuation = true;
	std::function<void()> send;
	send = [&]() {
		drain.Run(
			[&]() {
				if (sent < 25) {
					++sent;
					send();
				}
			},
			[&](auto continuation) {
				if (rejectContinuation) {
					throw std::runtime_error("schedule failed");
				}
				events.emplace_back(std::move(continuation));
			});
	};
	bool threw = false;
	try {
		send();
	} catch (const std::runtime_error &) {
		threw = true;
	}
	ASSERT_TRUE(threw);
	ASSERT_EQUALS(8, sent);
	rejectContinuation = false;
	send();
	ASSERT_EQUALS(16, sent);
	ASSERT_EQUALS(1u, events.size());
	while (!events.empty()) {
		RunNext(events);
	}
	ASSERT_EQUALS(25, sent);
}

TEST(ECNotificationDrain, ExceptionsRestoreTheDispatchDepth)
{
	CECNotificationDrain drain;
	EventQueue events;
	for (int attempt = 0; attempt < 10; ++attempt) {
		bool threw = false;
		try {
			drain.Run([]() { throw std::runtime_error("send failed"); },
				[&events](
					auto continuation) { events.emplace_back(std::move(continuation)); });
		} catch (const std::runtime_error &) {
			threw = true;
		}
		ASSERT_TRUE(threw);
		ASSERT_TRUE(events.empty());
	}
}

TEST(ECNotificationDrain, TimerResumesBatchesThroughTheNativeEventLoop)
{
	wxEventLoop loop;
	wxEventLoopActivator active(&loop);
	CECNotificationDrain drain;
	CECNotificationTimer timer;
	int sent = 0;
	std::function<void()> send;
	send = [&]() {
		drain.Run(
			[&]() {
				if (sent < 25) {
					++sent;
					if (sent == 25) {
						loop.ScheduleExit(0);
					}
					send();
				}
			},
			[&](auto continuation) { ASSERT_TRUE(timer.Schedule(std::move(continuation))); });
	};
	send();
	ASSERT_EQUALS(8, sent);
	ASSERT_TRUE(timer.IsRunning());
	// Processing posted callbacks must not consume the remaining batches.
	wxTheApp->ProcessPendingEvents();
	ASSERT_EQUALS(8, sent);
	CLoopExitTimer timeout(loop, 1);
	ASSERT_TRUE(timeout.StartOnce(2000));
	ASSERT_EQUALS(0, loop.Run());
	ASSERT_EQUALS(25, sent);
	ASSERT_FALSE(timer.IsRunning());
}

TEST(ECNotificationDrain, CancellingTheTimerDropsItsContinuation)
{
	wxEventLoop loop;
	wxEventLoopActivator active(&loop);
	CECNotificationTimer timer;
	bool resumed = false;
	ASSERT_TRUE(timer.Schedule([&resumed]() { resumed = true; }));
	timer.Cancel();
	CLoopExitTimer stop(loop, 0);
	ASSERT_TRUE(stop.StartOnce(20));
	ASSERT_EQUALS(0, loop.Run());
	ASSERT_FALSE(resumed);
	ASSERT_FALSE(timer.IsRunning());
}

TEST(ECNotificationDrain, DestroyingTheTimerDropsItsContinuation)
{
	wxEventLoop loop;
	wxEventLoopActivator active(&loop);
	bool resumed = false;
	{
		CECNotificationTimer timer;
		ASSERT_TRUE(timer.Schedule([&resumed]() { resumed = true; }));
	}
	CLoopExitTimer stop(loop, 0);
	ASSERT_TRUE(stop.StartOnce(20));
	ASSERT_EQUALS(0, loop.Run());
	ASSERT_FALSE(resumed);
}
