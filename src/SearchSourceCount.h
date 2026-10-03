//
// This file is part of the aMule Project.
//
// Copyright (c) 2003-2026 aMule Team ( https://amule-org.github.io )
// Copyright (c) 2002-2011 Merkur ( devs@emule-project.net / http://www.emule-project.net )
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

#ifndef SEARCHSOURCECOUNT_H
#define SEARCHSOURCECOUNT_H

#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>

// Server reports contribute to an eD2k total; Kad reports repeat a network-wide
// estimate. The networks can overlap, so display the larger estimate, not their
// sum. Keep both contributions when merging rows and alternative filenames.
class CSearchSourceCount
{
public:
	CSearchSourceCount() = default;
	CSearchSourceCount(uint32_t count, bool kad)
	: m_ed2k(kad ? 0 : count)
	, m_kad(kad ? count : 0)
	{
	}

	void Merge(const CSearchSourceCount &other)
	{
		// Counts come from untrusted server reports. Keep availability monotonic
		// even when their sum exceeds the range of the EC/on-disk count fields.
		const uint32_t maximum = std::numeric_limits<uint32_t>::max();
		m_ed2k += std::min(other.m_ed2k, maximum - m_ed2k);
		m_kad = std::max(m_kad, other.m_kad);
	}

	uint32_t Total() const { return std::max(m_ed2k, m_kad); }
	uint32_t Ed2k() const { return m_ed2k; }
	uint32_t Kad() const { return m_kad; }

	static CSearchSourceCount FromNetworks(uint32_t ed2k, uint32_t kad)
	{
		CSearchSourceCount counts(ed2k, false);
		counts.m_kad = kad;
		return counts;
	}

private:
	uint32_t m_ed2k = 0;
	uint32_t m_kad = 0;
};

// Apply an EC update, not a new source report: values replace the supplied
// halves, including decreases and zero. An absent half retains its last value.
// Without an initial pair the split remains unknown rather than inventing zero.
inline bool UpdateSearchSourceCounts(
	std::optional<CSearchSourceCount> &counts, std::optional<uint32_t> ed2k, std::optional<uint32_t> kad)
{
	if ((!ed2k && !kad) || (!counts && (!ed2k || !kad))) {
		return false;
	}
	const uint32_t nextEd2k = ed2k ? *ed2k : counts->Ed2k();
	const uint32_t nextKad = kad ? *kad : counts->Kad();
	const bool changed = !counts || counts->Ed2k() != nextEd2k || counts->Kad() != nextKad;
	counts = CSearchSourceCount::FromNetworks(nextEd2k, nextKad);
	return changed;
}

#endif
