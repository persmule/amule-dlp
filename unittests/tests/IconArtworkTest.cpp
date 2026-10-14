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

// GUI rendering regression test. CI requires a display; local headless runs may skip.

#include <wx/app.h>
#include <wx/log.h>
#include <wx/frame.h>
#include <wx/mstream.h>
#include <wx/image.h>
#include <wx/settings.h>
#include <wx/menu.h>
#include <wx/filename.h>
#include "CamuleArtProvider.h"
#include "CountryFlags.h"
#include <vector>
#include "MenuIcons.h"
#include "icons/icon_data.h"
#include <iostream>
#include <stdexcept>
#include <string>
#include <cstring>
#include <cstdlib>

// Keep diagnostics visible to CTest without opening modal dialogs. Optional
// GUI startup may skip; once initialized, every warning or error is fatal.
class FailingLogStderr : public wxLogStderr
{
public:
	explicit FailingLogStderr(bool failImmediately = true)
	: m_failImmediately(failImmediately)
	{
	}

	void FinishInitialization()
	{
		m_failImmediately = true;
		if (m_failed) {
			Flush();
			std::exit(EXIT_FAILURE);
		}
	}

protected:
	void DoLogRecord(wxLogLevel level, const wxString &message, const wxLogRecordInfo &info) override
	{
		wxLogStderr::DoLogRecord(level, message, info);
		if (level == wxLOG_Error || level == wxLOG_Warning) {
			m_failed = true;
			if (m_failImmediately) {
				Flush();
				std::exit(EXIT_FAILURE);
			}
		}
	}

private:
	bool m_failImmediately;
	bool m_failed = false;
};

static void IconArtworkAssertHandler(
	const wxString &file, int line, const wxString &func, const wxString &cond, const wxString &msg)
{
	std::cerr << file.ToUTF8().data() << ":" << line << ": " << func.ToUTF8().data() << ": assertion "
		  << cond.ToUTF8().data() << " failed: " << msg.ToUTF8().data() << "\n";
	std::exit(EXIT_FAILURE);
}

class CheckApp : public wxApp
{
public:
	bool OnInit() override { return true; }
};
wxIMPLEMENT_APP_NO_MAIN(CheckApp);
void require(bool ok, const char *message)
{
	if (!ok)
		throw std::runtime_error(message);
}
int main(int argc, char **argv)
{
	// Cover startup without turning an optional headless skip into a failure.
	const bool requireGui = std::getenv("AMULE_ICON_TEST_REQUIRE_GUI") != nullptr;
	auto *log = new FailingLogStderr(requireGui);
	delete wxLog::SetActiveTarget(log);
	wxSetAssertHandler(IconArtworkAssertHandler);
	if (!wxEntryStart(argc, argv)) {
		std::cerr << "GUI initialization failed: no native rendering checks ran\n";
		return requireGui ? 1 : 77;
	}
	log->FinishInitialization();
	if (!wxTheApp->CallOnInit()) {
		wxEntryCleanup();
		return 1;
	}
	wxInitAllImageHandlers();
	wxArtProvider::Push(new CamuleArtProvider);
	int count = 0, flags = 0, menus = 0;
	const auto entries = amule_get_all_icons(&count);
	const char *output = std::getenv("AMULE_ICON_TEST_OUTPUT");
	if (output)
		wxFileName::Mkdir(wxString::FromUTF8(output), wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);
	int status = 0;
	try {
		// Prime wx's neutral cache, then switch menu colours without resetting
		// it. Live theme changes must affect both SVG and PNG menu rendering.
		wxArtProvider::GetBitmapBundle("amule:menu_pause_fill", wxART_MENU, wxSize(16, 16));
		for (bool preferSvg : { true, false }) {
			for (const wxColour &colour :
				{ wxColour(0, 0, 0), wxColour(255, 255, 255), wxColour(0, 0, 0) }) {
				const auto bundle = CamuleArtProvider::GetMenuBitmapBundle(
					"menu_pause_fill", wxSize(16, 16), colour, preferSvg);
				require(bundle.IsOk(), "theme-aware bundle invalid");
				const auto image = bundle.GetBitmap(wxSize(16, 16)).ConvertToImage();
				require(image.GetRed(5, 7) == colour.Red() &&
						image.GetGreen(5, 7) == colour.Green() &&
						image.GetBlue(5, 7) == colour.Blue(),
					"live theme switch kept stale menu colour");
			}
		}

		for (const char *code : { "an", "unknown" }) {
			for (int scale : { 1, 2, 3 }) {
				const auto entry = amule_find_icon((std::string("flag_") + code).c_str());
				require(entry != nullptr, "legacy flag missing");
				const auto data = scale == 3   ? entry->png3x_data
						  : scale == 2 ? entry->png2x_data
							       : entry->png_data;
				const auto length = scale == 3   ? entry->png3x_len
						    : scale == 2 ? entry->png2x_len
								 : entry->png_len;
				wxMemoryInputStream stream(data, length);
				wxImage image(stream, wxBITMAP_TYPE_PNG);
				require(image.IsOk() && image.GetSize() == wxSize(16 * scale, 12 * scale),
					"legacy flag wrong dimensions");
				require(image.HasAlpha(), "legacy flag padding lacks transparency");
				// The 1x original is padded; filtering of enlarged raster-only
				// artwork can blend the boundary, so require its last row only.
				for (int x = 0; x < 16 * scale; ++x) {
					require(image.GetAlpha(x, 12 * scale - 1) == 0,
						"legacy bottom row not transparent");
				}
			}
		}

		CCountryFlags cache;
		for (int i = 0; i < count; ++i) {
			const auto &entry = entries[i];
			bool flag = std::strncmp(entry.name, "flag_", 5) == 0;
			bool menu = std::strncmp(entry.name, "menu_", 5) == 0;
			if (!flag && !menu)
				continue;
			if (flag)
				++flags;
			else
				++menus;
			for (double scale : { 1.0, 1.5, 2.0, 3.0, 4.0 }) {
				wxBitmap b;
				// wxMSW scales DIP into drawing coordinates before GetFlag;
				// its logical bitmap dimensions are physical pixels. GTK/macOS
				// instead use a backing scale with fixed logical dimensions.
#ifdef __WXMSW__
				const wxSize flagSize(wxRound(16 * scale), wxRound(12 * scale));
				const double contentScale = 1.0;
#else
				const wxSize flagSize(16, 12);
				const double contentScale = scale;
#endif
				if (flag)
					b = cache.GetFlag(
						wxString::FromUTF8(entry.name + 5), flagSize, contentScale);
				else {
					const auto bundle = wxArtProvider::GetBitmapBundle(
						wxString("amule:") + entry.name, wxART_MENU, wxSize(16, 16));
					require(bundle.IsOk(), "menu bundle invalid");
					b = bundle.GetBitmap(
						wxSize(wxRound(16 * scale), wxRound(16 * scale)));
				}
				require(b.IsOk(), "bitmap invalid");
				require(b.GetWidth() == wxRound(16 * scale), "wrong pixel width");
				if (flag)
					require(b.GetLogicalSize() == flagSize, "wrong logical size");
				auto image = b.ConvertToImage();
				if (output)
					require(image.SaveFile(wxFileName(wxString::FromUTF8(output),
								       wxString::Format("%s-%d.png",
									       entry.name,
									       wxRound(16 * scale)))
								       .GetFullPath(),
							wxBITMAP_TYPE_PNG),
						"PNG export failed");
			}
		}
		require(flags == 253, "embedded flag inventory incomplete");
		// Check source selection independently at each density. Compare after
		// native bitmap conversion to allow platform alpha premultiplication.
		CCountryFlags pngCache;
		const auto raster = amule_find_icon("flag_us");
		require(raster && raster->png2x_data && raster->png3x_data && !raster->svg_data,
			"PNG density table incomplete");
		for (double scale : { 1.0, 1.5, 2.0, 3.0, 4.0 }) {
			const unsigned char *data = scale > 2   ? raster->png3x_data
						    : scale > 1 ? raster->png2x_data
								: raster->png_data;
			const unsigned int length = scale > 2   ? raster->png3x_len
						    : scale > 1 ? raster->png2x_len
								: raster->png_len;
			wxMemoryInputStream stream(data, length);
			wxImage source(stream, wxBITMAP_TYPE_PNG);
			require(source.IsOk(), "density PNG decode failed");
			if (!source.HasAlpha()) {
				source.InitAlpha();
			}
			const wxSize pixels(wxRound(16 * scale), wxRound(12 * scale));
			const auto scaled = source.GetSize() == pixels
						    ? source
						    : source.Scale(pixels.x, pixels.y, wxIMAGE_QUALITY_HIGH);
			wxBitmap expectedBitmap(scaled);
			expectedBitmap.SetScaleFactor(scale);
			const auto expected = expectedBitmap.ConvertToImage();
			const auto actual = pngCache.GetFlag("us", wxSize(16, 12), scale).ConvertToImage();
			require(actual.IsOk() && expected.IsOk() && actual.GetSize() == pixels,
				"density rendering failed");
			if (scale == 1.5) {
				const auto nearest =
					wxBitmap(source.Scale(pixels.x, pixels.y, wxIMAGE_QUALITY_NEAREST))
						.ConvertToImage();
				require(std::memcmp(nearest.GetData(),
						expected.GetData(),
						pixels.x * pixels.y * 3) != 0,
					"fixture cannot distinguish box filter from nearest neighbour");
			}
			require(std::memcmp(actual.GetData(), expected.GetData(), pixels.x * pixels.y * 3) ==
					0,
				"wrong source density or PNG resampling");
			require(actual.HasAlpha() == expected.HasAlpha() &&
					(!expected.HasAlpha() || std::memcmp(actual.GetAlpha(),
									 expected.GetAlpha(),
									 pixels.x * pixels.y) == 0),
				"resampling changed alpha");
		}
		const auto cached = cache.GetFlag("us", wxSize(16, 12), 2);
		const auto cachedAgain = cache.GetFlag("us", wxSize(16, 12), 2);
		require(cached.GetRefData() == cachedAgain.GetRefData(),
			"flag pixels copied on repeated draw");
		require(!cache.GetFlag("us", wxSize(0, 12), 1).IsOk(), "invalid flag size accepted");
		const auto unknown = cache.GetFlag("unknown", wxSize(16, 12), 1);
		const auto missing = cache.GetFlag("zz", wxSize(16, 12), 1);
		require(unknown.GetRefData() == missing.GetRefData(), "unknown code missed bitmap cache");
		require(std::memcmp(unknown.ConvertToImage().GetData(),
				missing.ConvertToImage().GetData(),
				16 * 12 * 3) == 0,
			"unknown fallback changed");
		// Windows converts DIP to drawing coordinates; backing scale remains one.
		auto windows = cache.GetFlag("us", wxSize(32, 24), 1);
		require(windows.GetLogicalSize() == wxSize(32, 24), "Windows DIP sizing");
		// The same bundle must render correctly after a monitor transition back to 1x.
		auto back = cache.GetFlag("us", wxSize(16, 12), 1);
		require(back.GetSize() == wxSize(16, 12), "monitor transition sizing");
		wxMenu menu;
		auto item = AppendMenuIcon(&menu, 1234, "&Pause", MenuIcon::Pause);
		require(item->GetId() == 1234 && item->GetItemLabel() == "&Pause",
			"menu command or mnemonic changed");
		require(item->GetBitmapBundle().IsOk(), "menu bitmap missing");
#ifdef __WXOSX_COCOA__
		require(LastMenuIconIsTemplate(menu), "Cocoa menu image not marked as a template");
#endif
		item->Enable(false);
		require(!item->IsEnabled(), "disabled state");
		auto check = menu.AppendCheckItem(1235, "Auto");
		check->Check();
		require(check->IsChecked(), "check state");
#ifdef __WXOSX_COCOA__
		const wxColour textColour = *wxBLACK;
#else
		const auto textColour = wxSystemSettings::GetColour(wxSYS_COLOUR_MENUTEXT);
#endif
		const auto pause = item->GetBitmapBundle().GetBitmap(wxSize(16, 16)).ConvertToImage();
		require(pause.GetRed(5, 7) == textColour.Red() &&
				pause.GetGreen(5, 7) == textColour.Green() &&
				pause.GetBlue(5, 7) == textColour.Blue(),
			"menu theme colour");
	} catch (const std::exception &e) {
		std::cerr << e.what() << "\n";
		status = 1;
	}
	wxArtProvider::Pop();
	wxTheApp->OnExit();
	wxEntryCleanup();
	if (!status)
		std::cout << "PASS: " << flags << " flags and " << menus
			  << " menu icons at 1x/1.5x/2x/3x/4x; fallback, bitmap cache, DPI transition, menu "
			     "semantics and live theme "
			     "colour\n";
	return status;
}
