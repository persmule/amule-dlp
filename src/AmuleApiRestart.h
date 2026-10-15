// This file is part of the aMule Project.
// Copyright (c) 2003-2026 aMule Team ( https://amule-org.github.io )
// Licensed under the GNU General Public License, version 2 or later.

#ifndef AMULE_API_RESTART_H
#define AMULE_API_RESTART_H

#include <cstdint>

// Monotonic uptime in milliseconds. Only a failed supervised child (or a failed
// retry launch) arms this policy; successful launches consume the pending retry.
class AmuleApiRestart
{
public:
	void Started(uint64_t now)
	{
		m_started = now;
		m_running = true;
		m_due = 0;
	}

	void Failed(uint64_t now)
	{
		// A minute of stable operation ends the previous crash loop.
		if (m_running && now - m_started >= 60000) {
			m_delay = 0;
		}
		m_running = false;
		m_delay = m_delay == 0 ? 5000 : (m_delay < 30000 ? m_delay * 2 : 60000);
		m_due = now + m_delay;
	}

	void Cancel()
	{
		m_running = false;
		m_due = 0;
		m_delay = 0;
	}

	bool IsDue(uint64_t now) const { return m_due != 0 && now >= m_due; }
	uint64_t DelayMs() const { return m_delay; }

private:
	uint64_t m_started = 0;
	uint64_t m_due = 0;
	uint64_t m_delay = 0;
	bool m_running = false;
};

#endif // AMULE_API_RESTART_H
