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

#include <muleunit/test.h>
#include "BlockReservation.h"
#include <algorithm>
#include <limits>
#include <vector>

using namespace muleunit;
namespace
{
std::optional<CBlockRange> Find(CBlockRange candidate, const std::vector<CBlockRange> &reservations)
{
	return FindUnreservedBlock(candidate, reservations, [](CBlockRange range) { return range; });
}
} // namespace
DECLARE_SIMPLE(BlockReservation)

TEST(BlockReservation, PartialReservationDoesNotHideTheRestOfTheBlock)
{
	const auto suffix = Find({ 0, 99 }, { { 0, 19 } });
	ASSERT_TRUE(suffix.has_value());
	ASSERT_EQUALS(20u, suffix->start);
	ASSERT_EQUALS(99u, suffix->end);
	const auto prefix = Find({ 0, 99 }, { { 20, 39 } });
	ASSERT_TRUE(prefix.has_value());
	ASSERT_EQUALS(0u, prefix->start);
	ASSERT_EQUALS(19u, prefix->end);
}

TEST(BlockReservation, UnorderedOverlappingReservationsAreClipped)
{
	const auto result = Find({ 10, 99 }, { { 40, 59 }, { 15, 29 }, { 0, 19 }, { 25, 34 } });
	ASSERT_TRUE(result.has_value());
	ASSERT_EQUALS(35u, result->start);
	ASSERT_EQUALS(39u, result->end);
	ASSERT_FALSE(Find({ 10, 99 }, { { 50, 99 }, { 0, 59 } }).has_value());
}

TEST(BlockReservation, InclusiveBoundariesCannotOverflow)
{
	const uint64_t maximum = std::numeric_limits<uint64_t>::max();
	ASSERT_FALSE(Find({ maximum, maximum }, { { maximum, maximum } }).has_value());
	const auto last = Find({ maximum - 1, maximum }, { { maximum - 1, maximum - 1 } });
	ASSERT_TRUE(last.has_value());
	ASSERT_EQUALS(maximum, last->start);
	ASSERT_EQUALS(maximum, last->end);
	const auto first = Find({ 0, 1 }, { { 1, 1 } });
	ASSERT_TRUE(first.has_value());
	ASSERT_EQUALS(0u, first->end);
	ASSERT_FALSE(Find({ 1, 0 }, {}).has_value());
}

TEST(BlockReservation, AllocatesEveryFreeByteExactlyOnceRegardlessOfReservationOrder)
{
	// Compare the production selector against a byte-level oracle over every occupancy mask.
	for (unsigned mask = 0; mask < 256; ++mask) {
		std::vector<CBlockRange> reservations;
		for (unsigned bit = 0; bit < 8; ++bit) {
			if ((mask & (1u << bit)) != 0) {
				reservations.push_back({ bit, bit });
			}
		}
		std::reverse(reservations.begin(), reservations.end());
		unsigned allocated = mask;
		while (const auto result = Find({ 0, 7 }, reservations)) {
			ASSERT_TRUE(result->start <= result->end);
			ASSERT_TRUE(result->end < 8);
			for (uint64_t byte = result->start; byte <= result->end; ++byte) {
				const unsigned flag = 1u << byte;
				ASSERT_EQUALS(0u, allocated & flag);
				allocated |= flag;
			}
			reservations.push_back(*result);
		}
		ASSERT_EQUALS(255u, allocated);
	}
}

TEST(BlockReservation, IgnoresMalformedAndUnrelatedReservations)
{
	const auto result = Find({ 10, 19 }, { { 9, 1 }, { 0, 9 }, { 20, 99 } });
	ASSERT_TRUE(result.has_value());
	ASSERT_EQUALS(10u, result->start);
	ASSERT_EQUALS(19u, result->end);
}

TEST(BlockReservation, OverlappingIntervalsMatchFirstFreeRunOracle)
{
	// Every candidate and pair of intervals in an eight-byte window, in both
	// reservation orders. Repeat near UINT64_MAX to check inclusive arithmetic.
	for (const uint64_t base : { uint64_t{ 0 }, std::numeric_limits<uint64_t>::max() - 7 }) {
		std::vector<CBlockRange> ranges;
		for (unsigned start = 0; start < 8; ++start) {
			for (unsigned end = start; end < 8; ++end) {
				ranges.push_back({ base + start, base + end });
			}
		}
		for (const auto candidate : ranges) {
			for (const auto first : ranges) {
				for (const auto second : ranges) {
					std::vector<CBlockRange> reservations{ first, second };
					// Use small relative offsets so the independent byte oracle
					// never needs an overflowing UINT64_MAX + 1 sentinel.
					unsigned start = static_cast<unsigned>(candidate.start - base);
					const unsigned limit = static_cast<unsigned>(candidate.end - base);
					const auto reserved = [&](unsigned byte) {
						const uint64_t offset = base + byte;
						return (first.start <= offset && offset <= first.end) ||
						       (second.start <= offset && offset <= second.end);
					};
					while (start <= limit && reserved(start)) {
						++start;
					}
					unsigned end = start;
					while (end < limit && !reserved(end + 1)) {
						++end;
					}
					const auto result = Find(candidate, reservations);
					ASSERT_EQUALS(start <= limit, result.has_value());
					if (result) {
						ASSERT_EQUALS(base + start, result->start);
						ASSERT_EQUALS(base + end, result->end);
					}
				}
			}
		}
	}
}
