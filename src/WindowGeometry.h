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

#ifndef WINDOW_GEOMETRY_H
#define WINDOW_GEOMETRY_H

#include <wx/gdicmn.h>

#include <algorithm>
#include <cstdint>
#include <vector>

inline const wxRect *FindPrimaryWorkArea(const std::vector<wxRect> &workAreas, size_t primaryDisplay)
{
	if (primaryDisplay < workAreas.size() && !workAreas[primaryDisplay].IsEmpty()) {
		return &workAreas[primaryDisplay];
	}
	for (const wxRect &area : workAreas) {
		if (!area.IsEmpty()) {
			return &area;
		}
	}
	return nullptr;
}

// Preserve windows with a usable title-bar area, including those spanning monitors.
// The caller supplies the reachability margin in window coordinates (converted from DIP).
// Otherwise recover onto the most-overlapped work area, or the primary display
// when no work area overlaps. Use wide arithmetic for untrusted config values.
inline wxRect ConstrainWindowRect(const wxRect &saved,
	const std::vector<wxRect> &workAreas,
	const wxSize &minimum,
	int reachableMargin,
	size_t primaryDisplay = 0)
{
	if (workAreas.empty()) {
		return saved;
	}
	const wxRect *chosen = FindPrimaryWorkArea(workAreas, primaryDisplay);
	int64_t largest = 0;
	for (const wxRect &area : workAreas) {
		if (area.IsEmpty()) {
			continue;
		}
		const int64_t right = std::min(int64_t(saved.x) + saved.width, int64_t(area.x) + area.width);
		const int64_t width = std::max<int64_t>(0, right - std::max(saved.x, area.x));
		if (width >= reachableMargin && saved.y >= area.y &&
			int64_t(saved.y) <= int64_t(area.y) + area.height - reachableMargin) {
			return wxRect(saved.GetPosition(),
				wxSize(std::max(saved.width, minimum.x), std::max(saved.height, minimum.y)));
		}
		const int64_t bottom =
			std::min(int64_t(saved.y) + saved.height, int64_t(area.y) + area.height);
		const int64_t height = std::max<int64_t>(0, bottom - std::max(saved.y, area.y));
		if (width * height > largest) {
			largest = width * height;
			chosen = &area;
		}
	}
	if (!chosen) {
		return saved;
	}
	const int width = std::min(std::max(saved.width, minimum.x), chosen->width);
	const int height = std::min(std::max(saved.height, minimum.y), chosen->height);
	const int x = static_cast<int>(
		std::clamp<int64_t>(saved.x, chosen->x, int64_t(chosen->x) + chosen->width - width));
	const int y = static_cast<int>(
		std::clamp<int64_t>(saved.y, chosen->y, int64_t(chosen->y) + chosen->height - height));
	return wxRect(x, y, width, height);
}

struct CWindowRestoreOptions
{
	bool hasSavedPosition = false;
	bool overridePosition = false;
	bool overrideSize = false;
	bool useWorkAreaForMissingSize = false;
};

// Resolve defaults and saved preferences before constraining the window. Command-line
// components retain precedence independently, including when recovery changes the other axis.
inline wxRect RestoreWindowRect(const wxRect &current,
	const wxRect &saved,
	const std::vector<wxRect> &workAreas,
	const wxSize &minimum,
	int reachableMargin,
	size_t primaryDisplay,
	const CWindowRestoreOptions &options)
{
	wxRect desired = current;
	if (!options.overrideSize) {
		if (saved.width > 0 && saved.height > 0) {
			desired.SetSize(saved.GetSize());
		} else if (!options.overridePosition && options.useWorkAreaForMissingSize) {
			const wxRect *area = FindPrimaryWorkArea(workAreas, primaryDisplay);
			if (area) {
				desired = *area;
			}
		}
	}
	if (!options.overridePosition && options.hasSavedPosition) {
		desired.SetPosition(saved.GetPosition());
	}
	wxRect restored = ConstrainWindowRect(desired, workAreas, minimum, reachableMargin, primaryDisplay);
	if (options.overridePosition) {
		restored.SetPosition(current.GetPosition());
	}
	if (options.overrideSize) {
		restored.SetSize(current.GetSize());
	}
	return restored;
}

// Normal bounds and the visible restore state have separate lifetimes. Neither
// maximized dimensions nor minimized sentinel coordinates may replace normal bounds.
class CWindowGeometry
{
public:
	void Remember(const wxRect &rect, bool maximized, bool iconized)
	{
		if (iconized || (!maximized && rect.IsEmpty())) {
			return;
		}
		m_maximized = maximized;
		if (!maximized) {
			m_normal = rect;
			m_valid = true;
		}
	}
	bool IsValid() const { return m_valid; }
	const wxRect &GetNormalRect() const { return m_normal; }
	bool IsMaximized() const { return m_maximized; }

private:
	wxRect m_normal;
	bool m_valid = false;
	bool m_maximized = false;
};

#endif // WINDOW_GEOMETRY_H
