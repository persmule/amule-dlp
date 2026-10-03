//
// This file is part of the aMule Project.
//
// Copyright (c) 2004-2011 Angel Vidal ( kry@amule.org )
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

#include <wx/menu.h>
#include <wx/intl.h>
#include <wx/artprov.h>  // Needed for wxArtProvider
#include <wx/image.h>    // Needed for wxImage
#include <wx/settings.h> // Needed for wxSystemSettings

#include "MuleNotebook.h" // Interface declarations

#include <common/MenuIDs.h>

wxDEFINE_EVENT(wxEVT_COMMAND_MULENOTEBOOK_PAGE_CLOSING, wxEvent);
wxDEFINE_EVENT(wxEVT_COMMAND_MULENOTEBOOK_ALL_PAGES_CLOSED, wxEvent);
wxBEGIN_EVENT_TABLE(CMuleNotebook, wxNotebook)
	EVT_RIGHT_DOWN(CMuleNotebook::OnRMButton)

	EVT_MENU(MP_CLOSE_TAB, CMuleNotebook::OnPopupClose)
	EVT_MENU(MP_CLOSE_ALL_TABS, CMuleNotebook::OnPopupCloseAll)
	EVT_MENU(MP_CLOSE_OTHER_TABS, CMuleNotebook::OnPopupCloseOthers)

	// Madcat - tab closing engine
	EVT_LEFT_DOWN(CMuleNotebook::OnMouseButton)
	EVT_LEFT_UP(CMuleNotebook::OnMouseButton)
	EVT_MIDDLE_DOWN(CMuleNotebook::OnMouseButton)
	EVT_MIDDLE_UP(CMuleNotebook::OnMouseButton)
	EVT_MOTION(CMuleNotebook::OnMouseMotion)
	EVT_LEAVE_WINDOW(CMuleNotebook::OnMouseLeave)
wxEND_EVENT_TABLE()

wxBitmap ThemedCloseIcon(const wxSize &size)
{
	wxBitmap bmp = wxArtProvider::GetBitmap(wxART_CLOSE, wxART_OTHER, size);
	if (!bmp.IsOk() || !wxSystemSettings::GetAppearance().IsDark()) {
		return bmp;
	}
	// src/msw/artmsw.cpp handles no wxART_CLOSE, so MSW falls through to artstd.cpp's
	// art/close.xpm -- an X hardcoded to black, which vanishes against a dark tab. GTK maps
	// the id to the icon theme's window-close and already answers in the right colour, so
	// there the replacement below finds nothing to do.
	wxImage img = bmp.ConvertToImage();
	if (img.HasMask()) {
		// Mask to alpha first: Replace() matches on colour, so with the mask still in place
		// it would repaint the transparent pixels too and yield a solid white block.
		img.InitAlpha();
	}
	img.Replace(0, 0, 0, 255, 255, 255);
	// Carry the scale factor across: the wxImage round-trip drops it, and a 2x icon rebuilt
	// as 1x draws at twice the size on a HiDPI display.
	return wxBitmap(img, -1, bmp.GetScaleFactor());
}

CMuleNotebook::CMuleNotebook(wxWindow *parent,
	wxWindowID id,
	const wxPoint &pos,
	const wxSize &size,
	long style,
	const wxString &name)
: wxNotebook(parent, id, pos, size, style, name)
{
	m_popup_enable = true;
	m_popup_widget = NULL;
}

CMuleNotebook::~CMuleNotebook()
{
	DeleteAllPages();
}

void CMuleNotebook::SetPageToolTip(size_t page, const wxString &text)
{
	wxCHECK_RET(page < GetPageCount(), "Invalid page for tab tooltip");
	m_pageTooltips[GetPage(page)] = text;
}

bool CMuleNotebook::DeletePage(int nPage)
{
	wxCHECK_MSG((nPage >= 0) && (nPage < (int)GetPageCount()),
		false,
		"Trying to delete invalid page-index in CMuleNotebook::DeletePage");
	if (!m_pageTooltips.empty()) {
		UnsetToolTip();
	}
	m_pageTooltips.erase(GetPage(nPage));
	m_tabDownIcon = m_tabDownMiddle = -1;

	wxNotebookEvent evt(wxEVT_COMMAND_MULENOTEBOOK_PAGE_CLOSING, GetId(), nPage);
	evt.SetEventObject(this);
	ProcessEvent(evt);

	bool result = wxNotebook::DeletePage(nPage);

	if (GetPageCount() && (int)GetSelection() >= (int)GetPageCount()) {
		SetSelection(GetPageCount() - 1);
	}

	// Send a page change event to work around a wx problem when the newly selected page is
	// identical with the deleted page: wx sends a page change event during deletion, but the
	// control is still the one to be deleted at that moment.
	if (GetPageCount()) {
		// Select the tab that took the place of the one we just deleted.
		size_t page = nPage;
		// Except if we deleted the last one - then select the one that is last now.
		if (page == GetPageCount()) {
			page--;
		}
		wxNotebookEvent event(wxEVT_NOTEBOOK_PAGE_CHANGED, GetId(), page);
		event.SetEventObject(this);
		ProcessEvent(event);
	} else {
		wxNotebookEvent event(wxEVT_COMMAND_MULENOTEBOOK_ALL_PAGES_CLOSED, GetId());
		event.SetEventObject(this);
		ProcessEvent(event);
	}

	return result;
}

bool CMuleNotebook::DeleteAllPages()
{
	Freeze();

	bool result = true;
	while (GetPageCount()) {
		result &= DeletePage(0);
	}

	Thaw();

	return result;
}

void CMuleNotebook::EnablePopup(bool enable)
{
	m_popup_enable = enable;
}

void CMuleNotebook::SetPopupHandler(wxWindow *widget)
{
	m_popup_widget = widget;
}

// #warning wxMac does not support selection by right-clicking on tabs!
void CMuleNotebook::OnRMButton(wxMouseEvent &event)
{
	if (!GetPageCount() || !m_popup_enable) {
		event.Skip();
		return;
	}

	// For some reason, gtk1 does a rather poor job when using the HitTest
	wxPoint eventPoint = event.GetPosition();

	int tab = HitTest(eventPoint);
	if (tab != wxNOT_FOUND) {
		SetSelection(tab);
	} else {
		event.Skip();
		return;
	}

	if (m_popup_widget) {
		wxMouseEvent evt = event;

		wxPoint point = evt.GetPosition();
		point = ClientToScreen(point);
		point = m_popup_widget->ScreenToClient(point);

		evt.m_x = point.x;
		evt.m_y = point.y;

		// Synchronous dispatch: the parent's handler is expected to call PopupMenu(), which
		// on wxGTK relies on the pointer grab from the current right-button-down event
		// still being active. AddPendingEvent queues the event for the next event-loop
		// cycle, and in amulegui the 1 Hz EC poll timer adds enough latency between the
		// queue insert and dispatch that the user's button-up arrives first ~80 % of the
		// time -- PopupMenu then opens and is immediately dismissed, looking like the menu
		// "does not latch". ProcessEvent runs the handler inline while the grab is fresh
		// (#680).
		m_popup_widget->GetEventHandler()->ProcessEvent(evt);
	} else {
		wxMenu menu(_("Close"));
		menu.Append(MP_CLOSE_TAB, wxString(_("Close tab")));
		menu.Append(MP_CLOSE_ALL_TABS, wxString(_("Close all tabs")));
		menu.Append(MP_CLOSE_OTHER_TABS, wxString(_("Close other tabs")));

		// Pop up at the pointer. On wxGTK the right-click lands on the tab strip, which
		// sits outside the client area PopupMenu() positions against, so
		// event.GetPosition() offset the menu upward by the tab-strip height. The default
		// position uses the cursor instead.
		PopupMenu(&menu);
	}
}

void CMuleNotebook::OnPopupClose(wxCommandEvent &WXUNUSED(evt))
{
	DeletePage(GetSelection());
}

void CMuleNotebook::OnPopupCloseAll(wxCommandEvent &WXUNUSED(evt))
{
	DeleteAllPages();
}

void CMuleNotebook::OnPopupCloseOthers(wxCommandEvent &WXUNUSED(evt))
{
	wxNotebookPage *current = GetPage(GetSelection());

	for (int i = GetPageCount() - 1; i >= 0; i--) {
		if (current != GetPage(i))
			DeletePage(i);
	}
}

bool CMuleNotebook::IsCloseIconHit(const wxPoint &position, int tab, long flags) const
{
	if (tab == wxNOT_FOUND || !(flags & wxNB_HITTEST_ONICON) || m_closeIconWidth < 0) {
		return false;
	}
	if (m_closeIconWidth == 0) {
		return true;
	}
	// wxNotebook exposes HitTest, but no portable image rectangle. Locate its
	// left edge with a bounded scan so the adjacent mode icon remains selectable.
	int width, height;
	if (!GetImageList() || !GetImageList()->GetSize(GetPageImage(tab), width, height)) {
		return false;
	}
	int left = position.x;
	for (int offset = 1; offset <= width; ++offset) {
		long neighborFlags = 0;
		const wxPoint neighbor(position.x - offset, position.y);
		if (HitTest(neighbor, &neighborFlags) != tab || !(neighborFlags & wxNB_HITTEST_ONICON)) {
			break;
		}
		left = neighbor.x;
	}
	return position.x - left < m_closeIconWidth;
}

void CMuleNotebook::OnMouseLeave(wxMouseEvent &event)
{
	if (!m_pageTooltips.empty()) {
		UnsetToolTip();
	}
	event.Skip();
}

void CMuleNotebook::OnMouseButton(wxMouseEvent &event)
{
	if (GetImageList() == NULL) {
		// This Mulenotebook has no images on tabs, so nothing to do.
		event.Skip();
		return;
	}

	const wxPoint position = event.GetPosition();
	long flags = 0;
	const int tab = HitTest(position, &flags);
	const bool onClose = IsCloseIconHit(position, tab, flags);

	if (event.LeftDown() && onClose) {
		m_tabDownIcon = tab;
	} else if (event.MiddleDown() && (tab != -1)) {
		// Anywhere on the tab, the 'x' included: it is the tab's image, not its label.
		m_tabDownMiddle = tab;
	} else if (event.LeftDown() || event.MiddleDown()) {
		m_tabDownIcon = -1;
		m_tabDownMiddle = -1;
	}

	if ((tab != -1) && ((onClose && event.LeftUp() && (tab == m_tabDownIcon)) ||
				   (event.MiddleUp() && (tab == m_tabDownMiddle)))) {
		// User did click on a 'x' or middle click on the tab
		m_tabDownIcon = -1;
		m_tabDownMiddle = -1;
		DeletePage(tab);
	} else {
		// Is not a 'x'. Send this event up.
		event.Skip();
	}
}

void CMuleNotebook::OnMouseMotion(wxMouseEvent &event)
{
	if (GetImageList() == NULL) {
		// This Mulenotebook has no images on tabs, so nothing to do.
		event.Skip();
		return;
	}

	const wxPoint position = event.GetPosition();
	long flags = 0;
	const int tab = HitTest(position, &flags);
	const bool onIcon = (tab != -1) && (flags == wxNB_HITTEST_ONICON);
	if (!m_pageTooltips.empty()) {
		wxString tip;
		if (IsCloseIconHit(position, tab, flags)) {
			tip = _("Close tab");
		} else if (tab != wxNOT_FOUND) {
			const auto found = m_pageTooltips.find(GetPage(tab));
			if (found != m_pageTooltips.end()) {
				tip = found->second;
			}
		}
		if (tip != GetToolTipText()) {
			if (tip.empty()) {
				UnsetToolTip();
			} else {
				SetToolTip(tip);
			}
		}
	}
	if (m_closeIconWidth != 0) {
		// Composite images already contain the close button: preserve their mode icon.
		event.Skip();
		return;
	}

	// Write only the images that actually change. SetPageImage() is a TCM_SETITEM on MSW, which
	// invalidates the tab it names, so setting every page on every motion event kept the whole
	// tab bar repainting for as long as the pointer moved over it -- with enough tabs open that
	// reads as flicker (issue #951). The highlight itself changes at most twice per crossing of
	// a close icon.
	for (int i = 0; i < (int)GetPageCount(); ++i) {
		const int image = (onIcon && i == tab) ? 1 : 0;
		if (GetPageImage(i) != image) {
			SetPageImage(i, image);
		}
	}

	if (!onIcon) {
		// Is not a 'x'. Send this event up.
		event.Skip();
	}
}

// File_checked_for_headers
