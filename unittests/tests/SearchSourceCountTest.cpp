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
#include "SearchSourceCount.h"
#include "SearchSourceFormat.h"
#include <algorithm>
#include <array>
#include <limits>

using namespace muleunit;
DECLARE_SIMPLE(SearchSourceCount)

TEST(SearchSourceCount, MixedReportsAreIndependentOfArrivalOrder)
{
	const std::array<CSearchSourceCount, 4> reports{ CSearchSourceCount(10, false),
		CSearchSourceCount(15, false),
		CSearchSourceCount(20, true),
		CSearchSourceCount(20, true) };
	std::array<int, 4> order{ 0, 1, 2, 3 };
	do {
		CSearchSourceCount total;
		for (int index : order) {
			total.Merge(reports[index]);
		}
		ASSERT_EQUALS(uint32_t(25), total.Total());
		ASSERT_EQUALS(uint32_t(25), total.Ed2k());
		ASSERT_EQUALS(uint32_t(20), total.Kad());
	} while (std::next_permutation(order.begin(), order.end()));
}

TEST(SearchSourceCount, FilenameGroupsRetainBothNetworkContributions)
{
	CSearchSourceCount firstName(10, false);
	firstName.Merge(CSearchSourceCount(20, true));
	CSearchSourceCount secondName(15, false);
	secondName.Merge(CSearchSourceCount(20, true));
	// Combining only the displayed child counts would incorrectly give 40.
	auto parent = firstName;
	parent.Merge(secondName);
	ASSERT_EQUALS(uint32_t(25), parent.Total());
	parent.Merge(CSearchSourceCount(30, true));
	ASSERT_EQUALS(uint32_t(30), parent.Total());
}

TEST(SearchSourceCount, SingleNetworkCountsKeepTheirExistingSemantics)
{
	CSearchSourceCount servers(10, false);
	servers.Merge(CSearchSourceCount(15, false));
	ASSERT_EQUALS(uint32_t(25), servers.Total());
	CSearchSourceCount kad(20, true);
	kad.Merge(CSearchSourceCount(20, true));
	kad.Merge(CSearchSourceCount(5, true));
	ASSERT_EQUALS(uint32_t(20), kad.Total());
	ASSERT_EQUALS(uint32_t(0), CSearchSourceCount().Total());
}

TEST(SearchSourceCount, ServerCountsSaturateInsteadOfWrapping)
{
	const uint32_t maximum = std::numeric_limits<uint32_t>::max();
	CSearchSourceCount servers(maximum - 5, false);
	servers.Merge(CSearchSourceCount(10, false));
	ASSERT_EQUALS(maximum, servers.Ed2k());
	ASSERT_EQUALS(maximum, servers.Total());
	servers.Merge(CSearchSourceCount(20, true));
	servers.Merge(CSearchSourceCount(1, false));
	ASSERT_EQUALS(maximum, servers.Total());
	ASSERT_EQUALS(uint32_t(20), servers.Kad());

	// Grouped filename totals use the same merge, including a saturated child.
	CSearchSourceCount parent(7, false);
	parent.Merge(servers);
	ASSERT_EQUALS(maximum, parent.Ed2k());
}

TEST(SearchSourceCount, CompactDisplay)
{
	ASSERT_EQUALS(
		wxString("E:10 K:50"), FormatSearchSources(50, CSearchSourceCount::FromNetworks(10, 50)));
	ASSERT_EQUALS(wxString("E:9"), FormatSearchSources(9, CSearchSourceCount::FromNetworks(9, 0)));
	ASSERT_EQUALS(wxString("K:50"), FormatSearchSources(50, CSearchSourceCount::FromNetworks(0, 50)));
	ASSERT_EQUALS(wxString("0"), FormatSearchSources(0, CSearchSourceCount()));
	ASSERT_EQUALS(wxString("50"), FormatSearchSources(50));
}

TEST(SearchSourceCount, TooltipExplainsCounts)
{
	ASSERT_EQUALS(wxString("Estimated availability: 9\neD2k sources: 9\nComplete sources: 8\nDirect "
			       "client endpoints: 1"),
		FormatSearchSourcesTooltip(9, 8, 1, CSearchSourceCount::FromNetworks(9, 0)));
	const wxString mixed = FormatSearchSourcesTooltip(50, 3, 2, CSearchSourceCount::FromNetworks(10, 50));
	ASSERT_TRUE(mixed.Contains("eD2k sources: 10"));
	ASSERT_TRUE(mixed.Contains("Kad sources: 50"));
	ASSERT_TRUE(mixed.Contains("Network counts may overlap"));
	ASSERT_TRUE(FormatSearchSourcesTooltip(50, 0, 0).Contains("Network breakdown unavailable."));
	// Remote results have no endpoint count; absence must not claim zero.
	ASSERT_FALSE(FormatSearchSourcesTooltip(50, 3, std::nullopt).Contains("Direct client endpoints:"));
	ASSERT_TRUE(FormatSearchSourcesTooltip(50, 3, 0).Contains("Direct client endpoints: 0"));
}

TEST(SearchSourceCount, IncrementalUpdatesRetainTheMissingHalf)
{
	std::optional<CSearchSourceCount> counts;
	ASSERT_TRUE(UpdateSearchSourceCounts(counts, 10, 50));
	ASSERT_TRUE(UpdateSearchSourceCounts(counts, 20, std::nullopt));
	ASSERT_EQUALS(uint32_t(20), counts->Ed2k());
	ASSERT_EQUALS(uint32_t(50), counts->Kad());
	ASSERT_TRUE(UpdateSearchSourceCounts(counts, std::nullopt, 60));
	ASSERT_EQUALS(uint32_t(20), counts->Ed2k());
	ASSERT_EQUALS(uint32_t(60), counts->Kad());
	ASSERT_EQUALS(wxString("E:20 K:60"), FormatSearchSources(counts->Total(), counts));
}

TEST(SearchSourceCount, IncrementalUpdatesReplaceRatherThanAccumulate)
{
	std::optional<CSearchSourceCount> counts = CSearchSourceCount::FromNetworks(20, 60);
	ASSERT_TRUE(UpdateSearchSourceCounts(counts, 5, std::nullopt));
	ASSERT_EQUALS(uint32_t(5), counts->Ed2k());
	ASSERT_EQUALS(uint32_t(60), counts->Kad());
	ASSERT_TRUE(UpdateSearchSourceCounts(counts, std::nullopt, 3));
	ASSERT_EQUALS(uint32_t(5), counts->Ed2k());
	ASSERT_EQUALS(uint32_t(3), counts->Kad());
	ASSERT_TRUE(UpdateSearchSourceCounts(counts, 0, std::nullopt));
	ASSERT_EQUALS(uint32_t(0), counts->Ed2k());
	ASSERT_EQUALS(uint32_t(3), counts->Kad());
	ASSERT_TRUE(UpdateSearchSourceCounts(counts, std::nullopt, 0));
	ASSERT_EQUALS(uint32_t(0), counts->Total());
}

TEST(SearchSourceCount, UnchangedUpdatesDoNotRequestRepaint)
{
	std::optional<CSearchSourceCount> counts = CSearchSourceCount::FromNetworks(10, 50);
	ASSERT_FALSE(UpdateSearchSourceCounts(counts, std::nullopt, std::nullopt));
	ASSERT_FALSE(UpdateSearchSourceCounts(counts, 10, std::nullopt));
	ASSERT_FALSE(UpdateSearchSourceCounts(counts, std::nullopt, 50));
	ASSERT_FALSE(UpdateSearchSourceCounts(counts, 10, 50));
	ASSERT_EQUALS(uint32_t(10), counts->Ed2k());
	ASSERT_EQUALS(uint32_t(50), counts->Kad());
}

TEST(SearchSourceCount, UnknownSplitRequiresAnInitialPair)
{
	std::optional<CSearchSourceCount> counts;
	ASSERT_FALSE(UpdateSearchSourceCounts(counts, std::nullopt, std::nullopt));
	ASSERT_FALSE(UpdateSearchSourceCounts(counts, 10, std::nullopt));
	ASSERT_FALSE(UpdateSearchSourceCounts(counts, std::nullopt, 50));
	ASSERT_FALSE(counts.has_value());
	// A known zero is different from unknown and must trigger the initial repaint.
	ASSERT_TRUE(UpdateSearchSourceCounts(counts, 0, 0));
	ASSERT_TRUE(counts.has_value());
	ASSERT_EQUALS(uint32_t(0), counts->Ed2k());
	ASSERT_EQUALS(uint32_t(0), counts->Kad());
	ASSERT_FALSE(UpdateSearchSourceCounts(counts, 0, 0));
}
