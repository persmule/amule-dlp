// This file is part of the aMule Project.
// Copyright (c) 2003-2026 aMule Team ( https://amule-org.github.io )
// Licensed under the GNU General Public License, version 2 or later.

#ifndef DISKSPACERESUMEBUDGET_H
#define DISKSPACERESUMEBUDGET_H

#include <cstdint>

// Share one free-space snapshot across active downloads and newly resumed files.
// Keep the existing strict threshold: some space must remain above the floor.
class CDiskSpaceResumeBudget
{
public:
	CDiskSpaceResumeBudget(uint64_t freeBytes, uint64_t minimumBytes)
	: m_available(freeBytes > minimumBytes ? freeBytes - minimumBytes : 0)
	{
	}

	bool Fits(uint64_t neededBytes) const { return neededBytes < m_available; }

	void Charge(uint64_t neededBytes)
	{
		m_available = neededBytes < m_available ? m_available - neededBytes : 0;
	}

private:
	uint64_t m_available;
};

#endif // DISKSPACERESUMEBUDGET_H
