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

// CListColumnStore maps a column's id to its persistence key and default width. The key is the on-
// disk config format (TableWidths*/TableOrdering*), so the id a caller registers has to be the id
// it can look the column back up by -- if the store renumbered stored ids, a saved width would come
// back attached to a different column, silently.
//
// These tests pin that the registry is a plain sorted insert. Registering out of order is the case
// that matters: it never happens in the app today, because every list registers its columns in
// ascending order, which is exactly why a renumbering bug in this path could sit here unnoticed.

#include <muleunit/test.h>

#include <ListColumnStore.h>
#include <wx/fileconf.h>
#include <wx/mstream.h>
#include <limits>

using namespace muleunit;

DECLARE_SIMPLE(CListColumnStore)

TEST(CListColumnStore, RegistrationOutOfOrderKeepsEveryColumnAddressable)
{
	// The middle insert: 1 lands between columns already registered, the shape a renumbering
	// insert would corrupt -- moving 2 and 3 out from under the callers that still ask for them
	// by those ids.
	//
	// Two entries have to follow the insertion point for that to be visible. With only one, an
	// off-by-one in such a loop skips the single displaced entry and the damage lands somewhere
	// unobservable instead.
	CListColumnStore store;
	store.RegisterColumn(0, 250, "N");
	store.RegisterColumn(2, 100, "Z");
	store.RegisterColumn(3, 90, "Y");
	store.RegisterColumn(1, 70, "P");

	ASSERT_EQUALS(wxString("N"), store.GetColumnName(0));
	ASSERT_EQUALS(wxString("P"), store.GetColumnName(1));
	ASSERT_EQUALS(wxString("Z"), store.GetColumnName(2));
	ASSERT_EQUALS(wxString("Y"), store.GetColumnName(3));

	ASSERT_EQUALS(250, store.GetColumnDefaultWidth(0));
	ASSERT_EQUALS(70, store.GetColumnDefaultWidth(1));
	ASSERT_EQUALS(100, store.GetColumnDefaultWidth(2));
	ASSERT_EQUALS(90, store.GetColumnDefaultWidth(3));
}

TEST(CListColumnStore, KeyLookupSurvivesOutOfOrderRegistration)
{
	// The reverse direction, which is how a saved config finds its column:
	// LoadSettings() reads a key and calls GetColumnIndex() to place it.
	CListColumnStore store;
	store.RegisterColumn(0, 250, "N");
	store.RegisterColumn(2, 100, "Z");
	store.RegisterColumn(1, 70, "P");

	ASSERT_EQUALS(0, store.GetColumnIndex("N"));
	ASSERT_EQUALS(1, store.GetColumnIndex("P"));
	ASSERT_EQUALS(2, store.GetColumnIndex("Z"));
}

TEST(CListColumnStore, AscendingRegistrationIsUnchanged)
{
	// What every list in the app actually does; guards the common path against
	// a regression in the insert itself.
	CListColumnStore store;
	store.RegisterColumn(0, 400, "N");
	store.RegisterColumn(1, 100, "Z");
	store.RegisterColumn(2, 90, "Y");

	ASSERT_EQUALS(wxString("N"), store.GetColumnName(0));
	ASSERT_EQUALS(wxString("Z"), store.GetColumnName(1));
	ASSERT_EQUALS(wxString("Y"), store.GetColumnName(2));

	ASSERT_EQUALS(400, store.GetColumnDefaultWidth(0));
	ASSERT_EQUALS(100, store.GetColumnDefaultWidth(1));
	ASSERT_EQUALS(90, store.GetColumnDefaultWidth(2));

	ASSERT_EQUALS(0, store.GetColumnIndex("N"));
	ASSERT_EQUALS(1, store.GetColumnIndex("Z"));
	ASSERT_EQUALS(2, store.GetColumnIndex("Y"));
}

TEST(CListColumnStore, NonContiguousIdsAreKeptAsGiven)
{
	// Ids need not be dense: the store must not compact them into positions.
	CListColumnStore store;
	store.RegisterColumn(5, 120, "T");
	store.RegisterColumn(9, 130, "H");

	ASSERT_EQUALS(wxString("T"), store.GetColumnName(5));
	ASSERT_EQUALS(wxString("H"), store.GetColumnName(9));
	ASSERT_EQUALS(5, store.GetColumnIndex("T"));
	ASSERT_EQUALS(9, store.GetColumnIndex("H"));
}

TEST(CListColumnStore, UnknownColumnsReportTheirMissValues)
{
	CListColumnStore store;
	store.RegisterColumn(0, 250, "N");

	ASSERT_TRUE(store.GetColumnName(7).IsEmpty());
	ASSERT_EQUALS(-1, store.GetColumnDefaultWidth(7)); // wxLIST_AUTOSIZE
	ASSERT_EQUALS(-1, store.GetColumnIndex("nope"));
}

namespace
{
class WidthConfig
{
public:
	WidthConfig()
	: input("", 0)
	, config(input)
	, previous(wxConfigBase::Set(&config))
	{
	}
	~WidthConfig() { wxConfigBase::Set(previous); }
	wxMemoryInputStream input;
	wxFileConfig config;
	wxConfigBase *previous;
};
class WidthProvider : public IColumnWidthProvider
{
public:
	explicit WidthProvider(double scale)
	: scale(scale)
	{
	}
	int GetColumnCount() const override { return 2; }
	int GetColumnWidth(int col) const override { return widths[col]; }
	bool SetColumnWidth(int col, int width) override
	{
		widths[col] = width;
		return true;
	}
	double GetWidthScale() const override { return scale; }
	double scale;
	int widths[2] = { 200, 100 };
};
void RegisterWidths(CListColumnStore &store)
{
	store.SetTableName("Test");
	store.RegisterColumn(0, 200, "N");
	store.RegisterColumn(1, 100, "S");
}
} // namespace
TEST(CListColumnStore, WidthsAndHiddenCacheSurviveScaleChange)
{
	WidthConfig fixture;
	CListColumnStore store;
	RegisterWidths(store);
	WidthProvider before(2);
	before.widths[0] = 600;
	before.widths[1] = 0;
	store.SetCachedWidth(1, 240);
	store.SaveSettings(before, {});
	CListColumnStore restored;
	RegisterWidths(restored);
	WidthProvider after(1);
	CListColumnStore::CSortingList order;
	ASSERT_TRUE(restored.LoadSettings(after, "", order));
	ASSERT_EQUALS(300, after.widths[0]);
	ASSERT_EQUALS(0, after.widths[1]);
	ASSERT_EQUALS(120, restored.GetCachedWidth(1));
	restored.SaveSettings(after, order);
	WidthProvider again(2);
	ASSERT_TRUE(restored.LoadSettings(again, "", order));
	ASSERT_EQUALS(600, again.widths[0]);
	ASSERT_EQUALS(240, restored.GetCachedWidth(1));
}
TEST(CListColumnStore, OlderClientEditsUseStoredScale)
{
	WidthConfig fixture;
	CListColumnStore store;
	RegisterWidths(store);
	WidthProvider before(1);
	store.SaveSettings(before, {});
	fixture.config.Write("/eMule/TableWidthsTest", "N:444,S:-160");
	WidthProvider after(2);
	CListColumnStore::CSortingList order;
	ASSERT_TRUE(store.LoadSettings(after, "", order));
	ASSERT_EQUALS(888, after.widths[0]);
	ASSERT_EQUALS(320, store.GetCachedWidth(1));
	ASSERT_EQUALS(0, after.widths[1]);
}
TEST(CListColumnStore, LegacyPixelsMigrateWithoutInventingOriginalScale)
{
	WidthConfig fixture;
	fixture.config.Write("/eMule/TableWidthsTest", "N:600,S:-240");
	CListColumnStore store;
	RegisterWidths(store);
	WidthProvider atTwo(2);
	CListColumnStore::CSortingList order;
	ASSERT_TRUE(store.LoadSettings(atTwo, "", order));
	ASSERT_EQUALS(600, atTwo.widths[0]);
	store.SaveSettings(atTwo, order);
	WidthProvider atOne(1);
	ASSERT_TRUE(store.LoadSettings(atOne, "", order));
	ASSERT_EQUALS(300, atOne.widths[0]);
	ASSERT_EQUALS(120, store.GetCachedWidth(1));
}
TEST(CListColumnStore, InvalidWidthsRetainDefaultsWithoutOverflow)
{
	WidthConfig fixture;
	CListColumnStore store;
	RegisterWidths(store);
	WidthProvider provider(2);
	CListColumnStore::CSortingList order;
	fixture.config.Write("/eMule/TableWidthsTest",
		"N:" + wxString::Format("%ld", std::numeric_limits<long>::min()) + ",S:not-a-width");
	ASSERT_FALSE(store.LoadSettings(provider, "", order));
	ASSERT_EQUALS(200, provider.widths[0]);
	ASSERT_EQUALS(100, provider.widths[1]);
	fixture.config.Write("/eMule/TableWidthsTest", "N:9999999999999999999999999999,S:999999");
	ASSERT_FALSE(store.LoadSettings(provider, "", order));
	ASSERT_EQUALS(200, provider.widths[0]);
}
TEST(CListColumnStore, PreNamedColumnFormatStillRestoresPixels)
{
	WidthConfig fixture;
	fixture.config.Write("/eMule/TableWidthsTest", "500,90");
	// Stale metadata cannot give a pre-named-column profile an original scale.
	fixture.config.Write("/eMule/TableWidthsScaleTest", 1.0);
	CListColumnStore store;
	RegisterWidths(store);
	WidthProvider provider(2);
	CListColumnStore::CSortingList order;
	ASSERT_TRUE(store.LoadSettings(provider, "N,S", order));
	ASSERT_EQUALS(500, provider.widths[0]);
	ASSERT_EQUALS(90, provider.widths[1]);
}

TEST(CListColumnStore, SameFractionalScalePreservesOddPixelWidths)
{
	WidthConfig fixture;
	CListColumnStore store;
	RegisterWidths(store);
	WidthProvider provider(1.5);
	provider.widths[0] = 301;
	provider.widths[1] = 0;
	store.SetCachedWidth(1, 241);
	store.SaveSettings(provider, {});
	ASSERT_EQUALS(wxString("N:301,S:-241"), fixture.config.Read("/eMule/TableWidthsTest", ""));
	double scale = 0.0;
	ASSERT_TRUE(fixture.config.Read("/eMule/TableWidthsScaleTest", &scale));
	ASSERT_TRUE(scale == 1.5);
	CListColumnStore::CSortingList order;
	for (int restart = 0; restart < 5; ++restart) {
		CListColumnStore restored;
		RegisterWidths(restored);
		WidthProvider after(1.5);
		ASSERT_TRUE(restored.LoadSettings(after, "", order));
		ASSERT_EQUALS(301, after.widths[0]);
		ASSERT_EQUALS(0, after.widths[1]);
		ASSERT_EQUALS(241, restored.GetCachedWidth(1));
		restored.SaveSettings(after, order);
	}
}

TEST(CListColumnStore, FractionalScaleChangesRoundOnlyOnce)
{
	WidthConfig fixture;
	CListColumnStore store;
	RegisterWidths(store);
	WidthProvider before(1.5);
	before.widths[0] = 301;
	before.widths[1] = 0;
	store.SetCachedWidth(1, 241);
	store.SaveSettings(before, {});
	WidthProvider after(1.25);
	CListColumnStore::CSortingList order;
	ASSERT_TRUE(store.LoadSettings(after, "", order));
	ASSERT_EQUALS(251, after.widths[0]);
	ASSERT_EQUALS(201, store.GetCachedWidth(1));
	ASSERT_EQUALS(0, after.widths[1]);
	store.SaveSettings(after, order);
	WidthProvider back(1.5);
	ASSERT_TRUE(store.LoadSettings(back, "", order));
	ASSERT_EQUALS(301, back.widths[0]);
	ASSERT_EQUALS(241, store.GetCachedWidth(1));
}

TEST(CListColumnStore, InvalidScalePreservesLegacyPixels)
{
	WidthConfig fixture;
	fixture.config.Write("/eMule/TableWidthsTest", "N:301,S:-241");
	CListColumnStore store;
	RegisterWidths(store);
	CListColumnStore::CSortingList order;
	for (const char *scale : { "0", "-1", "nan", "inf", "-inf", "garbage", "1e999" }) {
		fixture.config.Write("/eMule/TableWidthsScaleTest", scale);
		WidthProvider provider(1.5);
		ASSERT_TRUE(store.LoadSettings(provider, "", order));
		ASSERT_EQUALS(301, provider.widths[0]);
		ASSERT_EQUALS(241, store.GetCachedWidth(1));
		ASSERT_EQUALS(0, provider.widths[1]);
	}
}

TEST(CListColumnStore, ExtremeScalesAndWidthsCannotOverflow)
{
	WidthConfig fixture;
	CListColumnStore store;
	RegisterWidths(store);
	CListColumnStore::CSortingList order;
	fixture.config.Write("/eMule/TableWidthsTest", "N:301,S:-241");
	fixture.config.Write("/eMule/TableWidthsScaleTest", "1e-300");
	WidthProvider provider(1.5);
	ASSERT_FALSE(store.LoadSettings(provider, "", order));
	ASSERT_EQUALS(200, provider.widths[0]);
	ASSERT_EQUALS(100, provider.widths[1]);
	fixture.config.Write("/eMule/TableWidthsScaleTest", "1.5");
	WidthProvider extreme(1e300);
	ASSERT_FALSE(store.LoadSettings(extreme, "", order));
	ASSERT_EQUALS(200, extreme.widths[0]);
	ASSERT_EQUALS(100, extreme.widths[1]);
	fixture.config.Write("/eMule/TableWidthsTest", "N:6145,S:-6145");
	ASSERT_FALSE(store.LoadSettings(provider, "", order));
	fixture.config.Write("/eMule/TableWidthsScaleTest", "1e300");
	fixture.config.Write("/eMule/TableWidthsTest",
		"N:" + wxString::Format("%ld", std::numeric_limits<long>::min()) +
			",S:" + wxString::Format("%ld", std::numeric_limits<long>::min()));
	ASSERT_FALSE(store.LoadSettings(provider, "", order));
}

TEST(CListColumnStore, SaveRemovesObsoleteCompanions)
{
	WidthConfig fixture;
	fixture.config.Write("/eMule/TableWidthsDIPTest", "N:999,S:-999");
	fixture.config.Write("/eMule/TableWidthsDIPSourceTest", "N:200,S:100");
	CListColumnStore store;
	RegisterWidths(store);
	WidthProvider provider(1);
	store.SaveSettings(provider, {});
	ASSERT_FALSE(fixture.config.HasEntry("/eMule/TableWidthsDIPTest"));
	ASSERT_FALSE(fixture.config.HasEntry("/eMule/TableWidthsDIPSourceTest"));
}

TEST(CListColumnStore, InvalidProviderScaleUsesIdentity)
{
	for (double scale : { 0.0,
		     -1.0,
		     std::numeric_limits<double>::quiet_NaN(),
		     std::numeric_limits<double>::infinity() }) {
		WidthConfig fixture;
		CListColumnStore store;
		RegisterWidths(store);
		WidthProvider before(scale);
		before.widths[0] = 301;
		store.SaveSettings(before, {});
		double savedScale = 0.0;
		ASSERT_TRUE(fixture.config.Read("/eMule/TableWidthsScaleTest", &savedScale));
		ASSERT_TRUE(savedScale == 1.0);
		CListColumnStore::CSortingList order;
		WidthProvider after(scale);
		ASSERT_TRUE(store.LoadSettings(after, "", order));
		ASSERT_EQUALS(301, after.widths[0]);
	}
}

TEST(CListColumnStore, ObsoleteCompanionsCannotOverridePixels)
{
	WidthConfig fixture;
	fixture.config.Write("/eMule/TableWidthsTest", "N:301,S:-241");
	fixture.config.Write("/eMule/TableWidthsDIPTest", "N:999,S:-999");
	fixture.config.Write("/eMule/TableWidthsDIPSourceTest", "N:301,S:-241");
	CListColumnStore store;
	RegisterWidths(store);
	WidthProvider provider(1.5);
	CListColumnStore::CSortingList order;
	ASSERT_TRUE(store.LoadSettings(provider, "", order));
	ASSERT_EQUALS(301, provider.widths[0]);
	ASSERT_EQUALS(241, store.GetCachedWidth(1));
}
