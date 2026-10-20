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

#ifndef DISPLAY_RATE_COUNTER_H
#define DISPLAY_RATE_COUNTER_H

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <deque>
#include <limits>
#include <memory>
#include <mutex>

// One-second byte window. Producers record monotonic millisecond timestamps; the core
// publishes a snapshot after processing transfers. Readers (including sort predicates)
// see that snapshot until the next core tick, without expiring samples themselves.
class CDisplayRateCounter
{
public:
	void Add(uint64_t bytes, uint64_t now) { AddBatch(bytes, now, now); }

	// Upload counters are drained once per core tick. Keep the interval start so
	// four 300 ms batches are not reported as just one second of traffic.
	void AddBatch(uint64_t bytes, uint64_t start, uint64_t now)
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		Advance(now);
		// A producer may have captured its timestamp before a newer sample or publish.
		// Preserve the original time: moving old bytes forward extends their lifetime.
		if (!bytes || m_now - now >= 1000) {
			return;
		}
		if (!m_samples) {
			m_samples = std::make_unique<std::deque<Sample>>();
		}
		auto pos = std::lower_bound(
			m_samples->begin(), m_samples->end(), now, [](const Sample &sample, uint64_t time) {
				return sample.time < time;
			});
		if (pos != m_samples->end() && pos->time == now) {
			pos->bytes += bytes;
			pos->start = std::min(pos->start, start);
		} else {
			m_samples->insert(pos, { now, bytes, std::min(start, now) });
		}
		m_total += bytes;
	}

	void CalculateRate(uint64_t now)
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		Advance(now);
		// The oldest upload batch can start before the window boundary. Normalize
		// by its full covered interval; packet samples still use exactly one second.
		const uint64_t elapsed =
			m_samples ? std::max<uint64_t>(1000, m_now - m_samples->front().start) : 1000;
		m_rate.store(elapsed == 1000 ? m_total
					     : static_cast<uint64_t>(
						       static_cast<long double>(m_total) * 1000 / elapsed),
			std::memory_order_relaxed);
	}

	uint64_t GetRate() const { return m_rate.load(std::memory_order_relaxed); }

	// Peer speeds and EC integer speed fields are limited to 32 bits.
	static uint32_t ClampRate(uint64_t rate)
	{
		return static_cast<uint32_t>(std::min<uint64_t>(rate, std::numeric_limits<uint32_t>::max()));
	}

	void Reset()
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_samples.reset();
		m_total = 0;
		m_now = 0;
		m_rate.store(0, std::memory_order_relaxed);
	}

private:
	struct Sample
	{
		uint64_t time;
		uint64_t bytes;
		uint64_t start;
	};
	void Advance(uint64_t now)
	{
		m_now = std::max(now, m_now);
		while (m_samples && !m_samples->empty() && m_now - m_samples->front().time >= 1000) {
			m_total -= m_samples->front().bytes;
			m_samples->pop_front();
		}
		if (m_samples && m_samples->empty()) {
			m_samples.reset();
		}
	}
	std::mutex m_mutex;
	// Most known peers never upload: allocate history only while bytes are in the window.
	std::unique_ptr<std::deque<Sample>> m_samples;
	uint64_t m_total = 0;
	uint64_t m_now = 0;
	std::atomic<uint64_t> m_rate{ 0 };
};

#endif // DISPLAY_RATE_COUNTER_H
