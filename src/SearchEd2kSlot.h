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

#ifndef SEARCH_ED2K_SLOT_H
#define SEARCH_ED2K_SLOT_H

#include <cstdint>

// The remote GUI keeps the last owner even after completion so an accepted
// replacement can invalidate that tab's reusable request before the next poll.
class CSearchEd2kSlot
{
public:
	uint32_t Accept(uint32_t searchID, bool active = true)
	{
		const uint32_t previous = m_owner;
		m_owner = searchID;
		m_active = active;
		return previous;
	}

	void Observe(uint32_t searchID, bool active)
	{
		if (active) {
			m_owner = searchID;
			m_active = true;
		} else if (m_owner == searchID) {
			m_active = false;
		}
	}

	void ObserveLegacyProgress(uint32_t searchID, uint32_t progress)
	{
		if (progress == 0xffff || progress == 0xfffe) {
			Observe(searchID, false);
		}
	}

	bool IsActive(uint32_t searchID) const { return m_active && m_owner == searchID; }

private:
	uint32_t m_owner = 0;
	bool m_active = false;
};

#endif
