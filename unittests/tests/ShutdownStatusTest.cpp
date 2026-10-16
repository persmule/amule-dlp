// Copyright (c) 2026 aMule Team
// SPDX-License-Identifier: GPL-2.0-or-later
#include <wx/wx.h>

#include "InternalEvents.h"
#include "ECNotificationDrain.h"
#include "ShutdownStatus.h"
#include "CamuleArtProvider.h"

#include <iostream>
#include <stdexcept>

class ShutdownStatusTestApp : public wxApp
{
public:
	bool OnInit() override
	{
		wxInitAllImageHandlers();
		wxArtProvider::Push(new CamuleArtProvider());
		return true;
	}
};

wxIMPLEMENT_APP_NO_MAIN(ShutdownStatusTestApp);

static void Require(bool condition, const char *message)
{
	if (!condition) {
		throw std::runtime_error(message);
	}
}

int main(int argc, char **argv)
{
	if (!wxEntryStart(argc, argv)) {
		return 77;
	}
	int result = 0;
	if (!wxTheApp->CallOnInit()) {
		result = 1;
	} else {
		bool internalRan = false;
		bool callbackRan = false;
		bool timerRan = false;
		bool notificationRan = false;
		bool checked = false;
		bool queryVetoed = false;
		bool queryHandlerRan = false;
		bool endSessionRan = false;
		bool teardownCallbackRan = false;
		wxTheApp->Bind(wxEVT_QUERY_END_SESSION, [&](wxCloseEvent &) { queryHandlerRan = true; });
		wxTheApp->Bind(wxEVT_END_SESSION, [&](wxCloseEvent &) { endSessionRan = true; });
		const wxEventTypeTag<CMuleInternalEvent> internalType(wxNewEventType());
		wxTheApp->Bind(internalType, [&](CMuleInternalEvent &) { internalRan = true; });
		{
			wxFrame anchor(nullptr, wxID_ANY, "Shutdown status regression test");
			anchor.Show();
			wxTheApp->SetTopWindow(&anchor);
			// Startup still permits dismissal; shutdown must keep the same splash
			// visible until its owner finishes saving and destroys it.
			{
				CSplashScreen startup(nullptr);
				startup.Show();
				startup.Close();
				if (startup.IsShown()) {
					std::cerr << "Startup splash dismissal changed\n";
					result = 1;
				}
			}
			wxTimer background;
			background.Bind(wxEVT_TIMER, [&](wxTimerEvent &) { timerRan = true; });
			wxTimer trigger;
			// Start from a native timer callback in a real running loop. Calling
			// YieldFor outside the loop misses GTK's native timer reentrancy.
			trigger.Bind(wxEVT_TIMER, [&](wxTimerEvent &) {
				checked = true;
				try {
					CShutdownStatus status;
					int paints = 0;
					status.Bind(wxEVT_PAINT, [&](wxPaintEvent &event) {
						++paints;
						// Model a native quit arriving inside YieldFor, bypassing
						// its event category mask. Swallowing a query without a
						// veto would let Cocoa call OnExit inside teardown.
						wxCloseEvent query(wxEVT_QUERY_END_SESSION);
						query.SetEventObject(wxTheApp);
						query.SetCanVeto(true);
						wxTheApp->ProcessEvent(query);
						queryVetoed = query.GetVeto();
						wxCloseEvent endSession(wxEVT_END_SESSION);
						endSession.SetEventObject(wxTheApp);
						wxTheApp->ProcessEvent(endSession);
						// Native loops can process pending wx events while yielding.
						// Exercise that path on every port, including GTK.
						wxTheApp->ProcessPendingEvents();
						event.Skip();
					});
					wxQueueEvent(wxTheApp, new CMuleInternalEvent(internalType));
					wxTheApp->CallAfter([&] { callbackRan = true; });
					CECNotificationTimer notifications;
					Require(notifications.Schedule([&] { notificationRan = true; }),
						"Could not arm the EC notification timer");
					background.StartOnce(1);
					wxMilliSleep(
						20); // Make the GLib/native timer ready before repainting.
					// Shutdown must cancel Notify() overrides before repainting;
					// those native callbacks bypass the wx event filter.
					notifications.Cancel();
					for (int i = 0; i < 100 && paints == 0; ++i) {
						status.SetStatus("Saving downloads...");
						wxMilliSleep(5);
					}
					Require(paints > 0, "Shutdown window did not repaint");
					Require(queryVetoed, "Native session-end query was not vetoed");
					Require(!queryHandlerRan && !endSessionRan,
						"Session-end handler reentered shutdown");
					Require(!notificationRan,
						"EC continuation ran during shutdown repaint");
					Require(!notifications.IsRunning(),
						"EC notification timer was not stopped");
					Require(!timerRan, "Native timer ran during shutdown repaint");
					Require(!internalRan, "Core event ran during shutdown repaint");
					Require(!callbackRan, "Queued callback ran during shutdown repaint");
					Require(status.IsShown() && !status.Close(),
						"Status window can be closed");
					// Drain once without a filter before core teardown, then
					// repaint later saves while callbacks remain suspended.
					wxTheApp->ProcessPendingEvents();
					Require(internalRan && callbackRan,
						"Pre-teardown pending events were not drained");
					wxTheApp->CallAfter([&] { teardownCallbackRan = true; });
					status.SetStatus("Flushing download buffers...");
					status.SetStatus("Finishing shutdown...");
					Require(status.IsShown(),
						"Splash closed before persistence finished");
					Require(!teardownCallbackRan, "Callback ran during core teardown");
				} catch (const std::exception &error) {
					std::cerr << error.what() << '\n';
					result = 1;
				}
				background.Stop();
				wxTheApp->ExitMainLoop();
			});
			trigger.StartOnce(1);
			wxTheApp->OnRun();
			if (!checked) {
				std::cerr << "Shutdown test timer did not run\n";
				result = 1;
			}
			// Pending core events and callbacks remain available after repainting.
			wxTheApp->ProcessPendingEvents();
			if (!internalRan || !callbackRan || !teardownCallbackRan) {
				std::cerr << "Pending events were lost: core=" << internalRan
					  << ", callback=" << callbackRan
					  << ", teardown=" << teardownCallbackRan << '\n';
				result = 1;
			}
			wxTheApp->SetTopWindow(nullptr);
		}
		wxTheApp->OnExit();
	}
	wxEntryCleanup();
	return result;
}
