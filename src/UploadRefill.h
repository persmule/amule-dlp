// Copyright (c) 2026 aMule Team
// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef UPLOADREFILL_H
#define UPLOADREFILL_H

#include <cassert>
#include <cstdint>

// Original (uncompressed) payload still in the socket queue, including the current packet.
// All access is protected by CEMSocket::m_sendLocker. Statistics resets do not affect it.
class UploadRefill
{
public:
	bool SetTarget(uint32_t target)
	{
		// A completion may have drained below a larger target before it was installed.
		// Recheck on another worker pass rather than waiting for a future crossing.
		const bool needsRefill = target > m_target && m_queued < target;
		m_target = target;
		return needsRefill;
	}
	void Queue(uint32_t payload) { m_queued += payload; }
	bool Complete(uint32_t payload)
	{
		const uint64_t before = m_queued;
		Discard(payload);
		return m_target != 0 && payload != 0 &&
		       ((before >= m_target && m_queued < m_target) || m_queued == 0);
	}
	void Discard(uint32_t payload)
	{
		assert(payload <= m_queued);
		m_queued -= payload;
	}
	void Clear()
	{
		m_queued = 0;
		m_target = 0;
	}

private:
	uint64_t m_queued = 0;
	uint32_t m_target = 0;
};
#endif
