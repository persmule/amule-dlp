// This file is part of the aMule Project.
// Copyright (c) 2003-2026 aMule Team ( https://amule-org.github.io )
// Licensed under the GNU General Public License, version 2 or later.

#ifndef KADPUBLISHCURSOR_H
#define KADPUBLISHCURSOR_H

#include <optional>

// Keep a key rather than an iterator so removals and reloads cannot invalidate the
// cursor. Each selection costs O(log N), with no walk from the start of the map.
template <typename Key> class CKadPublishCursor
{
public:
	template <typename Map> typename Map::const_iterator Next(const Map &files)
	{
		auto next = m_lastKey ? files.upper_bound(*m_lastKey) : files.begin();
		if (next == files.end()) {
			next = files.begin();
		}
		if (next != files.end()) {
			m_lastKey = next->first;
		}
		return next;
	}

private:
	std::optional<Key> m_lastKey;
};

#endif // KADPUBLISHCURSOR_H
