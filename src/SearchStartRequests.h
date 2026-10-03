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

#ifndef SEARCH_START_REQUESTS_H
#define SEARCH_START_REQUESTS_H

#include "SearchList.h"

#include <map>
#include <optional>
#include <vector>

// Keep cancellation attached to an optimistic ID until START supplies the daemon ID.
class CSearchStartRequests
{
public:
	struct Request
	{
		SearchType kind;
		bool stopRequested = false;
		bool closeRequested = false;
		bool ReplacesEd2kSlot() const
		{
			return kind == LocalSearch || kind == GlobalSearch || kind == AllSearch;
		}
	};

	void Begin(uint32_t id, SearchType kind) { m_starts.emplace(id, Request{ kind }); }

	bool DeferStop(uint32_t id, bool close)
	{
		const auto found = m_starts.find(id);
		if (found == m_starts.end()) {
			return false;
		}
		found->second.stopRequested = true;
		found->second.closeRequested |= close;
		return true;
	}

	std::optional<Request> Take(uint32_t id)
	{
		const auto found = m_starts.find(id);
		if (found == m_starts.end()) {
			return std::nullopt;
		}
		const Request request = found->second;
		m_starts.erase(found);
		return request;
	}

	// Defer discovery through the close round trip too: an older SEARCH_LIST reply
	// can still contain a just-closed ID until its CLOSE acknowledgment arrives.
	void BeginClose() { ++m_closes; }
	void FinishClose()
	{
		if (m_closes) {
			--m_closes;
		}
	}
	bool DiscoveryBlocked() const { return !m_starts.empty() || m_closes != 0; }

	std::vector<uint32_t> PendingIds() const
	{
		std::vector<uint32_t> ids;
		for (const auto &entry : m_starts) {
			ids.push_back(entry.first);
		}
		return ids;
	}

	void Reset()
	{
		m_starts.clear();
		m_closes = 0;
	}

private:
	std::map<uint32_t, Request> m_starts;
	size_t m_closes = 0;
};

#endif
