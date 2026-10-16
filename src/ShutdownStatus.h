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

#ifndef AMULE_SHUTDOWN_STATUS_H
#define AMULE_SHUTDOWN_STATUS_H

#include "SplashScreen.h"

#include <wx/app.h>
#include <wx/evtloop.h>
#include <wx/eventfilter.h>

// Shutdown owns this window through its explicit OnExit(), before wx can destroy
// top-level windows in CallOnExit(). Restrict repaint dispatch around partially
// destroyed core objects. Native sources calling an overridden wxTimer::Notify() directly can
// still run; the event filter cannot intercept those callbacks.
class CShutdownStatus : public CSplashScreen
{
public:
	CShutdownStatus()
	: CSplashScreen(nullptr, Phase::Shutdown)
	{
		Show();
	}

	void SetStatus(const wxString &status)
	{
		RepaintFilter filter(this);
		SetProgress(status, 0, true);
		// The owner calls OnExit directly and keeps the splash alive until cleanup
		// finishes. Supply a temporary loop if no active event loop is available.
		if (auto *loop = wxEventLoopBase::GetActive()) {
			loop->YieldFor(wxEVT_CATEGORY_UI);
		} else {
			wxEventLoop temporaryLoop;
			wxEventLoopActivator active(&temporaryLoop);
			temporaryLoop.YieldFor(wxEVT_CATEGORY_UI);
		}
		Update();
	}

private:
	// GTK's nested native loop dispatches GLib timer sources even when YieldFor
	// requests only UI events. Block wx event handlers outside this window while
	// repainting, including normal timer events and application events. This does
	// not block timer subclasses that override Notify() without sending an event.
	class RepaintFilter : public wxEventFilter
	{
	public:
		explicit RepaintFilter(wxWindow *window)
		: m_window(window)
		{
			// Native loops on some ports drain pending wx events while yielding.
			// Keep those events queued instead of consuming them in the filter.
			wxTheApp->SuspendProcessingOfPendingEvents();
			wxEvtHandler::AddFilter(this);
		}
		~RepaintFilter() override
		{
			wxEvtHandler::RemoveFilter(this);
			wxTheApp->ResumeProcessingOfPendingEvents();
		}

		int FilterEvent(wxEvent &event) override
		{
			// YieldFor can dispatch native quit/logoff requests even with a UI mask.
			// Merely swallowing the query lets the OS proceed to CallOnExit(),
			// destroying this window and reentering partially completed teardown.
			if (event.GetEventType() == wxEVT_QUERY_END_SESSION) {
				auto &query = static_cast<wxCloseEvent &>(event);
				if (query.CanVeto()) {
					query.Veto();
				}
				return Event_Processed;
			}
			for (auto *window = wxDynamicCast(event.GetEventObject(), wxWindow); window;
				window = window->GetParent()) {
				if (window == m_window) {
					return Event_Skip;
				}
			}
			return Event_Processed;
		}

	private:
		wxWindow *m_window;
	};
};

#endif
