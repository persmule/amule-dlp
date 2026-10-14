// This file is part of the aMule Project.
// Copyright (c) 2026 aMule Team ( https://amule-org.github.io )
// Licensed under GNU GPL version 2 or later; see COPYING.

#ifndef EC_NOTIFICATION_DRAIN_H
#define EC_NOTIFICATION_DRAIN_H

#include <memory>
#include <functional>
#include <utility>
#include <wx/timer.h>

// A timer yields to native events between batches. Repeated CallAfter callbacks
// could otherwise keep wxApp::ProcessPendingEvents running without returning.
class CECNotificationTimer : public wxTimer
{
public:
	bool Schedule(std::function<void()> continuation)
	{
		m_continuation = std::move(continuation);
		if (StartOnce(1)) {
			return true;
		}
		m_continuation = {};
		return false;
	}

	void Cancel()
	{
		Stop();
		m_continuation = {};
	}

	void Notify() override
	{
		auto continuation = std::move(m_continuation);
		m_continuation = {};
		if (continuation) {
			continuation();
		}
	}

private:
	std::function<void()> m_continuation;
};

// Main-thread-only dispatch policy for the recursive EC notification send chain.
// Deferred callbacks hold a weak state reference, so they cannot call a destroyed
// owner. The owner must cancel on connection loss and check output readiness in
// sendNext: a queued continuation must not add packets while output is blocked.
class CECNotificationDrain
{
public:
	CECNotificationDrain() = default;
	CECNotificationDrain(const CECNotificationDrain &) = delete;
	CECNotificationDrain &operator=(const CECNotificationDrain &) = delete;

	void Cancel()
	{
		if (m_state) {
			m_state->cancelled = true;
			m_state.reset();
		}
	}

	template <typename SendNext, typename Defer> void Run(SendNext sendNext, Defer defer)
	{
		Run(m_state, sendNext, defer);
	}

private:
	static constexpr unsigned MAX_DEPTH = 8;

	struct State
	{
		unsigned depth = 0;
		bool pending = false;
		bool cancelled = false;
	};

	template <typename SendNext, typename Defer>
	// Keep the state alive if sendNext cancels or destroys its owner.
	// NOLINTNEXTLINE(performance-unnecessary-value-param)
	static void Run(std::shared_ptr<State> state, SendNext sendNext, Defer defer)
	{
		if (!state || state->cancelled) {
			return;
		}
		if (state->depth >= MAX_DEPTH) {
			if (!state->pending) {
				state->pending = true;
				std::weak_ptr<State> lifetime = state;
				try {
					defer([lifetime, sendNext, defer]() {
						if (auto live = lifetime.lock()) {
							live->pending = false;
							Run(live, sendNext, defer);
						}
					});
				} catch (...) {
					state->pending = false;
					throw;
				}
			}
			return;
		}

		++state->depth;
		try {
			sendNext();
		} catch (...) {
			--state->depth;
			throw;
		}
		--state->depth;
	}

	std::shared_ptr<State> m_state = std::make_shared<State>();
};

#endif // EC_NOTIFICATION_DRAIN_H
