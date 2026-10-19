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
#include "WindowGeometry.h"
#include <limits>

using namespace muleunit;
DECLARE_SIMPLE(WindowGeometry)

TEST(WindowGeometry, RemovedMonitorAndNegativeCoordinatesRecover)
{
	const std::vector<wxRect> areas{ wxRect(0, 24, 1920, 1056) };
	const wxRect result =
		ConstrainWindowRect(wxRect(-5000, -4000, 800, 600), areas, wxSize(320, 240), 50);
	ASSERT_TRUE(areas[0].Contains(result));
	ASSERT_TRUE((wxSize(800, 600)) == (result.GetSize()));
}
TEST(WindowGeometry, ValidNegativeMonitorIsPreserved)
{
	const std::vector<wxRect> areas{ wxRect(0, 0, 1920, 1080), wxRect(-1280, -200, 1280, 1024) };
	const wxRect saved(-1200, -100, 900, 700);
	ASSERT_TRUE((saved) == (ConstrainWindowRect(saved, areas, wxSize(320, 240), 50)));
}
TEST(WindowGeometry, RemovedMonitorFallsBackToPrimaryDisplay)
{
	// Display enumeration order need not put the primary display first.
	const std::vector<wxRect> areas{ wxRect(-1280, 0, 1280, 1024), wxRect(0, 40, 1920, 1040) };
	const wxRect result =
		ConstrainWindowRect(wxRect(5000, 3000, 800, 600), areas, wxSize(320, 240), 50, 1);
	ASSERT_TRUE(areas[1].Contains(result));
	ASSERT_TRUE((wxSize(800, 600)) == (result.GetSize()));
}
TEST(WindowGeometry, ReachableSpanningWindowIsPreserved)
{
	const std::vector<wxRect> areas{ wxRect(0, 40, 1920, 1040), wxRect(1920, 0, 1280, 1024) };
	const wxRect saved(200, 80, 2800, 900);
	ASSERT_TRUE((saved) == (ConstrainWindowRect(saved, areas, wxSize(320, 240), 50)));
}
TEST(WindowGeometry, ReachableTopEdgeOnSecondaryDisplayIsPreserved)
{
	const std::vector<wxRect> areas{ wxRect(0, 0, 1920, 1080), wxRect(-1280, -1024, 1280, 1024) };
	const wxRect saved(-500, -800, 2400, 1800);
	ASSERT_TRUE((saved) == (ConstrainWindowRect(saved, areas, wxSize(320, 240), 50)));
}
TEST(WindowGeometry, ResizeBorderSliversRecoverOnEitherSide)
{
	const wxRect area(0, 0, 1441, 853);
	for (const int overlap : { 1, 7, 49 }) {
		for (const int x : { area.width - overlap, overlap - 900 }) {
			const wxRect saved(x, 100, 900, 600);
			const wxRect result = ConstrainWindowRect(saved, { area }, wxSize(320, 240), 50);
			ASSERT_TRUE(area.Contains(result));
			ASSERT_TRUE(saved.GetSize() == result.GetSize());
		}
	}
}
TEST(WindowGeometry, ExactHorizontalMarginPreservesBounds)
{
	const wxRect area(0, 0, 1441, 853);
	for (const int x : { area.width - 50, 50 - 900 }) {
		const wxRect saved(x, 100, 900, 600);
		ASSERT_TRUE(saved == ConstrainWindowRect(saved, { area }, wxSize(320, 240), 50));
	}
}
TEST(WindowGeometry, BottomMarginMustLeaveUsableTitleBar)
{
	const wxRect area(-1441, -853, 1441, 853);
	const wxRect saved(-1300, -50, 900, 600);
	ASSERT_TRUE(saved == ConstrainWindowRect(saved, { area }, wxSize(320, 240), 50));
	for (const int clearance : { 1, 7, 49 }) {
		const wxRect result = ConstrainWindowRect(
			wxRect(-1300, -clearance, 900, 600), { area }, wxSize(320, 240), 50);
		ASSERT_TRUE(area.Contains(result));
	}
}
TEST(WindowGeometry, ReachabilityMarginUsesCallerScale)
{
	const wxRect area(0, 0, 2882, 1706);
	const wxRect saved(2783, 100, 900, 600);
	ASSERT_TRUE(saved == ConstrainWindowRect(saved, { area }, wxSize(320, 240), 50));
	ASSERT_TRUE(area.Contains(ConstrainWindowRect(saved, { area }, wxSize(640, 480), 100)));
	const wxRect boundary(2782, 1606, 900, 600);
	ASSERT_TRUE(boundary == ConstrainWindowRect(boundary, { area }, wxSize(640, 480), 100));
	const wxRect tooLow(2782, 1607, 900, 600);
	ASSERT_TRUE(area.Contains(ConstrainWindowRect(tooLow, { area }, wxSize(640, 480), 100)));
}
TEST(WindowGeometry, UsableMarginOnAnotherMonitorPreservesSpanningBounds)
{
	const std::vector<wxRect> areas{ wxRect(0, 0, 1441, 853), wxRect(1441, 0, 1441, 853) };
	const wxRect saved(1434, 100, 900, 600);
	ASSERT_TRUE(saved == ConstrainWindowRect(saved, areas, wxSize(320, 240), 50));
	ASSERT_TRUE(areas[0].Contains(ConstrainWindowRect(saved, { areas[0] }, wxSize(320, 240), 50)));
}
TEST(WindowGeometry, RestorePassesReachabilityMargin)
{
	CWindowRestoreOptions options;
	options.hasSavedPosition = true;
	const wxRect area(0, 0, 1441, 853);
	const wxRect saved(1434, 100, 900, 600);
	ASSERT_TRUE((wxRect(541, 100, 900, 600)) ==
		    RestoreWindowRect(
			    wxRect(100, 80, 800, 600), saved, { area }, wxSize(320, 240), 50, 0, options));
}
TEST(WindowGeometry, UnreachableTopEdgeRecoversToOverlappingDisplay)
{
	const std::vector<wxRect> areas{ wxRect(0, 40, 1920, 1040), wxRect(1920, 0, 1280, 1024) };
	const wxRect result = ConstrainWindowRect(wxRect(2200, -100, 800, 600), areas, wxSize(320, 240), 50);
	ASSERT_TRUE((wxRect(2200, 0, 800, 600)) == (result));
}
TEST(WindowGeometry, BoundsIncludeSizeAndTaskbar)
{
	const wxRect area(0, 40, 1280, 680);
	const std::vector<wxRect> areas{ area };
	ASSERT_TRUE(
		(area) == (ConstrainWindowRect(wxRect(1200, 720, 8000, 4000), areas, wxSize(320, 240), 50)));
	ASSERT_TRUE((wxSize(320, 240)) ==
		    (ConstrainWindowRect(wxRect(10, 50, 1, 1), areas, wxSize(320, 240), 50).GetSize()));
}
TEST(WindowGeometry, ExtremeSavedCoordinatesDoNotOverflow)
{
	const wxRect area(0, 0, 1280, 720);
	ASSERT_TRUE((area) == (ConstrainWindowRect(wxRect(std::numeric_limits<int>::max(),
							   std::numeric_limits<int>::min(),
							   std::numeric_limits<int>::max(),
							   std::numeric_limits<int>::max()),
				      { area },
				      wxSize(320, 240),
				      50)));
}
TEST(WindowGeometry, EmptyPrimaryWorkAreaDoesNotCollapseWindow)
{
	const std::vector<wxRect> areas{ wxRect(), wxRect(100, 40, 1280, 720) };
	const wxRect result =
		ConstrainWindowRect(wxRect(-5000, -4000, 800, 600), areas, wxSize(320, 240), 50);
	ASSERT_TRUE(areas[1].Contains(result));
	ASSERT_TRUE((wxSize(800, 600)) == (result.GetSize()));
}
TEST(WindowGeometry, RecoveryUsesWideWorkAreaArithmetic)
{
	const int origin = std::numeric_limits<int>::max() - 99;
	const wxRect result = ConstrainWindowRect(
		wxRect(origin, -1000, 800, 600), { wxRect(origin, origin, 200, 200) }, wxSize(320, 240), 50);
	ASSERT_EQUALS(origin, result.x);
	ASSERT_EQUALS(origin, result.y);
	ASSERT_EQUALS(200, result.width);
	ASSERT_EQUALS(200, result.height);
}
TEST(WindowGeometry, MaximizedAndMinimizedStatesKeepNormalBounds)
{
	CWindowGeometry state;
	const wxRect normal(30, 40, 800, 600);
	state.Remember(normal, false, false);
	state.Remember(wxRect(0, 0, 1920, 1080), true, false);
	state.Remember(wxRect(-32000, -32000, 1, 1), false, true);
	ASSERT_TRUE((normal) == (state.GetNormalRect()));
	ASSERT_TRUE(state.IsMaximized());
	state.Remember(wxRect(50, 60, 900, 700), false, false);
	ASSERT_FALSE(state.IsMaximized());
	ASSERT_TRUE((wxRect(50, 60, 900, 700)) == (state.GetNormalRect()));
}

TEST(WindowGeometry, MinusOneIsAValidSavedCoordinate)
{
	CWindowRestoreOptions options;
	options.hasSavedPosition = true;
	const wxRect saved(-1, -1, 800, 600);
	ASSERT_TRUE(saved == RestoreWindowRect(wxRect(100, 80, 900, 700),
				     saved,
				     { wxRect(-1920, -1080, 3840, 2160) },
				     wxSize(320, 240),
				     50,
				     0,
				     options));
}
TEST(WindowGeometry, GtkMissingSizePreservesSavedPosition)
{
	CWindowRestoreOptions options;
	options.hasSavedPosition = true;
	options.useWorkAreaForMissingSize = true;
	const std::vector<wxRect> areas{ wxRect(0, 40, 1920, 1040), wxRect(-1280, -200, 1280, 1024) };
	const wxRect result = RestoreWindowRect(
		wxRect(0, 0, 800, 600), wxRect(-1100, -100, -1, -1), areas, wxSize(320, 240), 50, 0, options);
	ASSERT_TRUE((wxRect(-1100, -100, 1920, 1040)) == result);
}
TEST(WindowGeometry, GtkFirstRunUsesPrimaryWorkArea)
{
	CWindowRestoreOptions options;
	options.useWorkAreaForMissingSize = true;
	const std::vector<wxRect> areas{ wxRect(-1280, -200, 1280, 1024), wxRect(0, 40, 1920, 1040) };
	ASSERT_TRUE(areas[1] == RestoreWindowRect(wxRect(0, 0, 800, 600),
					wxRect(0, 0, -1, -1),
					areas,
					wxSize(320, 240),
					50,
					1,
					options));
}
TEST(WindowGeometry, GtkFirstRunSkipsEmptyWorkAreas)
{
	CWindowRestoreOptions options;
	options.useWorkAreaForMissingSize = true;
	const std::vector<wxRect> areas{ wxRect(), wxRect(100, 40, 1280, 720) };
	ASSERT_TRUE(areas[1] == RestoreWindowRect(wxRect(0, 0, 800, 600),
					wxRect(0, 0, -1, -1),
					areas,
					wxSize(320, 240),
					50,
					0,
					options));
}
TEST(WindowGeometry, ExplicitGeometryComponentsKeepIndependentPrecedence)
{
	const wxRect current(-5000, -4000, 1800, 1000);
	const wxRect saved(5000, 4000, 800, 600);
	const std::vector<wxRect> areas{ wxRect(100, 40, 1280, 680) };
	CWindowRestoreOptions options;
	options.hasSavedPosition = true;
	options.useWorkAreaForMissingSize = true;
	options.overridePosition = true;
	ASSERT_TRUE((wxRect(-5000, -4000, 800, 600)) ==
		    RestoreWindowRect(current, saved, areas, wxSize(320, 240), 50, 0, options));
	options.overrideSize = true;
	ASSERT_TRUE(current == RestoreWindowRect(current, saved, areas, wxSize(320, 240), 50, 0, options));
	options.overridePosition = false;
	ASSERT_TRUE((wxRect(100, 40, 1800, 1000)) ==
		    RestoreWindowRect(current, saved, areas, wxSize(320, 240), 50, 0, options));
}
TEST(WindowGeometry, MissingSavedPositionKeepsCurrentPosition)
{
	const wxRect current(100, 80, 800, 600);
	ASSERT_TRUE((wxRect(100, 80, 900, 700)) == RestoreWindowRect(current,
							   wxRect(-1, -1, 900, 700),
							   { wxRect(0, 0, 1920, 1080) },
							   wxSize(320, 240),
							   50,
							   0,
							   CWindowRestoreOptions()));
}
TEST(WindowGeometry, UnavailableDisplaysPreserveRequestedGeometry)
{
	const wxRect saved(-1200, -100, 900, 700);
	ASSERT_TRUE(saved == ConstrainWindowRect(saved, {}, wxSize(320, 240), 50));
	ASSERT_TRUE(saved ==
		    ConstrainWindowRect(saved, { wxRect(), wxRect(0, 0, -1, 100) }, wxSize(320, 240), 50));
}
TEST(WindowGeometry, TopEdgeOutsideWorkAreaRecovers)
{
	const wxRect area(0, 40, 1280, 680);
	ASSERT_TRUE((wxRect(100, 40, 800, 600)) ==
		    ConstrainWindowRect(wxRect(100, 39, 800, 600), { area }, wxSize(320, 240), 50));
	ASSERT_TRUE((wxRect(480, 80, 800, 600)) ==
		    ConstrainWindowRect(wxRect(1280, 80, 800, 600), { area }, wxSize(320, 240), 50));
}
TEST(WindowGeometry, InvalidNormalSampleKeepsRestoreState)
{
	CWindowGeometry state;
	ASSERT_FALSE(state.IsValid());
	state.Remember(wxRect(), false, false);
	ASSERT_FALSE(state.IsValid());
	state.Remember(wxRect(30, 40, 800, 600), false, false);
	state.Remember(wxRect(0, 0, 1920, 1080), true, false);
	state.Remember(wxRect(), false, false);
	ASSERT_TRUE(state.IsMaximized());
	ASSERT_TRUE((wxRect(30, 40, 800, 600)) == state.GetNormalRect());
}
