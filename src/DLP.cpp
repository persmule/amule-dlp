// Copyright (C) 2011 Bill Lee <bill.lee.y@gmail.com>, 2014 Persmule <persmule@gmail.com>
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
#include "Logger.h"

#include "DLP.h"
#include "antiLeech.h"
#include <wx/dynlib.h>                          /* Needed for wxDynamicLibrary */

#include "DLPPref.h"
#include "Preferences.h"	// Needed for CPreferences
#include "amule.h"		// Needed for theApp

#include <wx/stdpaths.h>                        /* Needed for wxStandardPaths */

#define PRE_CHECK(tag)	if( (!c->IsBanned()) && antiLeech && (thePrefs::GetDLPCheckMask() & tagn) )

void DLP::CheckHelloTag(CUpDownClient* c, UINT tagn){
	PRE_CHECK(PF_HELLOTAG){
		const wxChar* dlp_result = antiLeech->DLPCheckHelloTag(tagn);
		if(dlp_result != NULL) {
			wxString ret;
			ret.Printf(_("[HelloTag %s] %s"), dlp_result, c->GetClientFullInfo().c_str());
			c->Ban();
			theApp->AddDLPMessageLine(ret);
		}
	}
}

void DLP::CheckInfoTag(CUpDownClient* c, UINT tagn){
	PRE_CHECK(PF_INFOTAG){
		const wxChar* dlp_result = antiLeech->DLPCheckInfoTag(tagn);
		if(dlp_result != NULL) {
			wxString ret;
			ret.Printf(_("[InfoTag %s] %s"), dlp_result, c->GetClientFullInfo().c_str());
			c->Ban();
			theApp->AddDLPMessageLine(ret);
		}
	}
}

bool DLP::DLPCheck(CUpDownClient* c){
	const wxChar* tmp = NULL;
	wxString ret;
	
	unsigned int prefs = thePrefs::GetDLPCheckMask();

	CString modver(c->GetClientModString());
	CString clientver(c->GetClientVerString());
	CString uname(c->GetUserName());
	CString uhash(wxString(c->GetUserHash().EncodeSTL().c_str(), wxConvUTF8));
	
	//CheckGhostMod
	if(prefs & PF_GHOSTMOD) {
		if(c->HasNonOfficialOpCodes() && (modver.IsEmpty())) {
			ret = _("GhostMod");
			tmp = ret.c_str(); //char pointer
		}
	}

	// Check bad modstring
	if ((prefs & PF_MODSTRING) && (tmp == NULL)) {
		if((tmp = antiLeech->DLPCheckModstring_Soft(modver.c_str(), clientver.c_str())) == NULL)
			tmp = antiLeech->DLPCheckModstring_Hard(modver.c_str(), clientver.c_str());
	}
	/*
	if ((prefs & PF_USERHASH) && (tmp == NULL)) {
		// not finished
	}
	*/
	// Check bad username
	if ((prefs & PF_USERNAME) && (tmp == NULL)) {
		if ((tmp = antiLeech->DLPCheckNameAndHashAndMod(uname, uhash, modver)) == NULL){
			if( (tmp = antiLeech->DLPCheckUsername_Hard(uname.c_str())) == NULL )
				tmp = antiLeech->DLPCheckUsername_Soft(uname.c_str());
		}
	}
	

	// Check VeryCD eMule
	if ((prefs & PF_VERYCDEMULE) && (tmp == NULL)) {
		if(modver.Find(wxT("VeryCD")) != wxNOT_FOUND){
			ret = _("VeryCD Mod");
			tmp = ret.c_str();
		}
	}
	
	if (tmp != NULL) {
		ret = tmp;
		wxString wxInfo;
		wxInfo.Printf(wxT("[%s] %s"), ret.c_str(), c->GetClientFullInfo().c_str());
		c->Ban();
		theApp->AddDLPMessageLine(wxInfo);
		return true;
	}

	return false;

}

int DLP::ReloadAntiLeech(){
	//Unloading
	AddLogLineN(  _("Checking if there is a antiLeech working..."));
	if(antiLeechLib.IsLoaded()){
		Destoryer fn = (Destoryer)(antiLeechLib.GetSymbol( wxT("destoryAntiLeechInstant")));
		wxASSERT(fn);
		AddLogLineN(  _("Unload previous antiLeech..."));
		fn(antiLeech);
		antiLeech = NULL;
		antiLeechLib.Unload();
	}
	else
		AddLogLineN(  _("No working antiLeech exists."));
	//Get lib's location
	wxStandardPathsBase &spb(wxStandardPaths::Get());
#ifdef __WXMSW__
	wxString dataDir(spb.GetPluginsDir());
#elif defined(__WXMAC__)
	wxString dataDir(spb.GetDataDir());
#else
	wxString dataDir(spb.GetDataDir().BeforeLast(wxT('/')) + wxT("/amule"));
#endif
	wxString localName = wxDynamicLibrary::CanonicalizeName(wxT("antiLeech"));
	wxString systemwideFile(JoinPaths(dataDir, localName));
	wxString userFile(thePrefs::GetConfigDir() + localName);
	wxString fallbackFile(wxT("antiLeech"));
	//Try to load lib;
	AddLogLineN(  _("Trying to load antiLeech..."));
	if( !LoadFrom(userFile) ){
		if( !LoadFrom(systemwideFile) ){
			if( !LoadFrom(fallbackFile) ){
				AddLogLineC(  _("No antiLeech available!"));
				return 1;	//Not found
			}
		}
	}
	//Searching symbol "createAntiLeechInstant"
	Creator fn = (Creator)(antiLeechLib.GetSymbol( wxT("createAntiLeechInstant") ));
	if(!fn){
		antiLeechLib.Unload();
		AddLogLineC(  _("antiLeech found, but it seems not to be a valid antiLeech!"));
		return 2;	//Found, but isn't antiLeech
	}
	//Try to create antiLeech
	antiLeech = fn();
	if(antiLeech){
		wxString logline;
		logline.Printf(_("Succeed loading antiLeech! Version: %d"), antiLeech->GetDLPVersion());
		AddLogLineC( logline);
		return 0;
	}
	//else
	antiLeechLib.Unload();
	AddLogLineC(  _("FAIL! An error occur when setting up antiLeech."));
	return 3;	//Fail to create antiLeech instant

}

DLP::~DLP(){
	if(antiLeechLib.IsLoaded()){
		Destoryer fn = (Destoryer)(antiLeechLib.GetSymbol( wxT("destoryAntiLeechInstant")));
		wxASSERT(fn);
		AddLogLineN(  _("Unload previous antiLeech..."));
		fn(antiLeech);
		//antiLeech = NULL;
		//antiLeechLib.Unload();
	}
}

bool DLP::LoadFrom(wxString& file){
	antiLeechLib.Load(file);
	return antiLeechLib.IsLoaded();
}
