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

#ifndef BLOCKRESERVATION_H
#define BLOCKRESERVATION_H

#include <cstdint>
#include <optional>

struct CBlockRange
{
	uint64_t start;
	uint64_t end; // Inclusive, like the download gap and reservation lists.
};

// Find the first unreserved interval inside a candidate block. Reservations may arrive in
// any order and may overlap. ReadRange adapts the existing list without owning peer state.
// Never discard an entire block merely because another peer reserved part of it.
template <typename Reservations, typename ReadRange>
std::optional<CBlockRange> FindUnreservedBlock(
	CBlockRange candidate, const Reservations &reservations, ReadRange readRange)
{
	if (candidate.start > candidate.end) {
		return std::nullopt;
	}
	for (;;) {
		uint64_t end = candidate.end;
		bool advanced = false;
		for (const auto &reservation : reservations) {
			const CBlockRange reserved = readRange(reservation);
			if (reserved.start > reserved.end || reserved.end < candidate.start ||
				reserved.start > candidate.end) {
				continue;
			}
			if (reserved.start <= candidate.start) {
				if (reserved.end >= candidate.end) {
					return std::nullopt;
				}
				candidate.start = reserved.end + 1;
				advanced = true;
				break; // Restart: an earlier reservation may cover the new start.
			}
			if (reserved.start - 1 < end) {
				end = reserved.start - 1;
			}
		}
		if (!advanced) {
			return CBlockRange{ candidate.start, end };
		}
	}
}

#endif // BLOCKRESERVATION_H
