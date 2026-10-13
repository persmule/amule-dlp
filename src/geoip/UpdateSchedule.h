// This file is part of the aMule Project.
// Copyright (c) 2003-2026 aMule Team ( https://amule-org.github.io )
// Licensed under the GNU General Public License, version 2 or later.

#ifndef GEOIPUPDATESCHEDULE_H
#define GEOIPUPDATESCHEDULE_H

#include <cstdint>

// Wall-clock seconds, as in the daily version check: CLOCK_MONOTONIC stops while Linux is
// suspended. A clock jump wraps the unsigned difference into at most one early check.
// Attempts, including failures, restart the interval to avoid retry storms.
class CGeoIPUpdateSchedule
{
public:
	static constexpr uint64_t Interval = 7ULL * 24 * 60 * 60;
	explicit CGeoIPUpdateSchedule(uint64_t now = 0)
	: m_lastAttempt(now)
	{
	}

	bool IsDue(uint64_t now, bool enabled, bool autoUpdate, bool downloading) const
	{
		return enabled && autoUpdate && !downloading && now - m_lastAttempt >= Interval;
	}

	void RecordAttempt(uint64_t now) { m_lastAttempt = now; }

private:
	uint64_t m_lastAttempt;
};

#endif // GEOIPUPDATESCHEDULE_H
