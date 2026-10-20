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

#ifndef NETWORKPREFERENCES_H
#define NETWORKPREFERENCES_H

// Reconcile a checkbox with core updates without discarding an uncommitted user edit.
class CNetworkPreferenceEdit
{
public:
	void Reset(bool enabled)
	{
		m_synced = m_displayed = enabled;
		m_edited = false;
	}
	bool Merge(bool displayed, bool enabled)
	{
		// Only a new user change can clear an edit. A matching remote update must
		// not consume it: another client may change the value back before we save.
		if (displayed != m_displayed) {
			m_edited = displayed != m_synced;
		}
		m_displayed = m_edited ? displayed : enabled;
		m_synced = enabled;
		return m_displayed;
	}

private:
	bool m_synced = false;
	bool m_displayed = false;
	bool m_edited = false;
};

#endif // NETWORKPREFERENCES_H
