//								-*- C++ -*-
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

#ifndef KAD_LOOKUP_VIEW_H
#define KAD_LOOKUP_VIEW_H

#include <wx/wx.h>
#include <wx/weakref.h>

// Parent-owned, modeless snapshot: connection controls stay accessible while reading.
class CKadLookupView final : public wxDialog
{
	wxTextCtrl *m_text;

public:
	explicit CKadLookupView(wxWindow *parent)
	: wxDialog(parent,
		  wxID_ANY,
		  _("Kad lookup diagnostics"),
		  wxDefaultPosition,
		  parent->FromDIP(wxSize(850, 500)),
		  wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
	{
		auto *sizer = new wxBoxSizer(wxVERTICAL);
		m_text = new wxTextCtrl(this,
			wxID_ANY,
			wxEmptyString,
			wxDefaultPosition,
			wxDefaultSize,
			wxTE_MULTILINE | wxTE_READONLY | wxHSCROLL);
		sizer->Add(m_text, wxSizerFlags(1).Expand().Border(wxALL, FromDIP(8)));
		sizer->Add(CreateButtonSizer(wxOK), wxSizerFlags().Expand().Border(wxALL, FromDIP(8)));
		SetSizer(sizer);
		Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { Destroy(); }, wxID_OK);
		Bind(wxEVT_CLOSE_WINDOW, [this](wxCloseEvent &) { Destroy(); });
	}
	void SetSnapshot(const wxString &text) { m_text->ChangeValue(text); }
	wxString GetSnapshot() const { return m_text->GetValue(); }
};

#ifdef CLIENT_GUI
#include "libs/ec/cpp/RemoteConnect.h"
#include "kademlia/utils/LookupDiagnosticsEC.h"
#include <memory>

namespace KadLookupRemote
{
inline wxString FormatPacket(const CECPacket &packet)
{
	if (packet.GetOpCode() != EC_OP_GET_KAD_LOOKUPS) {
		return _("The core does not support Kad lookup diagnostics.");
	}
	Kademlia::LookupReports reports;
	if (!Kademlia::DecodeLookupDiagnosticsTags(packet, reports)) {
		return _("Invalid Kad lookup diagnostics reply.");
	}
	return Kademlia::FormatLookupDiagnostics(reports);
}
} // namespace KadLookupRemote

// Owned by the Kad page and shared with its outstanding reply handler. Closing
// the snapshot window does not end a request or permit another one.
class CKadLookupRequestState
{
	wxWeakRef<wxButton> m_button;
	bool m_supported = false;
	bool m_pending = false;
	void UpdateButton()
	{
		if (m_button) {
			m_button->Enable(m_supported && !m_pending);
		}
	}

public:
	explicit CKadLookupRequestState(wxButton *button)
	: m_button(button)
	{
	}
	void SetSupported(bool supported)
	{
		m_supported = supported;
		UpdateButton();
	}
	bool TryBegin()
	{
		if (!m_supported || m_pending) {
			return false;
		}
		m_pending = true;
		UpdateButton();
		return true;
	}
	void Complete()
	{
		m_pending = false;
		UpdateButton();
	}
};

class CKadLookupReply final : public CECPacketHandlerBase
{
	wxWeakRef<CKadLookupView> m_view;
	std::shared_ptr<CKadLookupRequestState> m_request;

public:
	CKadLookupReply(CKadLookupView *view, std::shared_ptr<CKadLookupRequestState> request)
	: m_view(view)
	, m_request(std::move(request))
	{
	}
	~CKadLookupReply() override { m_request->Complete(); }
	void HandlePacket(const CECPacket *packet) override
	{
		if (m_view) {
			m_view->SetSnapshot(KadLookupRemote::FormatPacket(*packet));
		}
		delete this;
	}
	void AbortPendingRequest() override
	{
		if (m_view) {
			m_view->SetSnapshot(
				_("Connection lost. Reconnect and request lookup diagnostics again."));
		}
		delete this;
	}
};
#endif
#endif
