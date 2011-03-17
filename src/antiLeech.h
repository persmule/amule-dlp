#ifndef ANTILEECH_H
#define ANTILEECH_H


#pragma once

#include "antiLeech_wx.h"
#include "CString_wx.h"

class IantiLeech 
{
public:
	virtual ~IantiLeech(){};                /* Bill Lee: Not be used currently */
	//BOOL WINAPI DllMain(HINSTANCE hinstDLL,DWORD,LPVOID);
	virtual DWORD GetDLPVersion() = 0;
	//old versions to keep compatible
	/* //drop old version support
	virtual LPCTSTR DLPCheckModstring(LPCTSTR modversion, LPCTSTR clientversion);
	virtual LPCTSTR DLPCheckUsername(LPCTSTR username);
	virtual LPCTSTR DLPCheckNameAndHash(CString username, CString& userhash);
	*/
	//new versions
	virtual LPCTSTR DLPCheckModstring_Hard(LPCTSTR modversion, LPCTSTR clientversion) = 0;
	virtual LPCTSTR DLPCheckModstring_Soft(LPCTSTR modversion, LPCTSTR clientversion) = 0;
	virtual LPCTSTR DLPCheckUsername_Hard(LPCTSTR username) = 0;
	virtual LPCTSTR DLPCheckUsername_Soft(LPCTSTR username) = 0;
	virtual LPCTSTR DLPCheckNameAndHashAndMod(const CString& username, const CString& userhash, const CString& modversion) = 0;
	virtual LPCTSTR DLPCheckMessageSpam(LPCTSTR messagetext) = 0;


	virtual LPCTSTR DLPCheckUserhash(const PBYTE userhash) = 0;


	virtual LPCTSTR DLPCheckHelloTag(UINT tagnumber) = 0;
	virtual LPCTSTR DLPCheckInfoTag(UINT tagnumber) = 0;

	//void  TestFunc();

//Bill Lee: no need in interface abstract class
//private:
//	static bool IsTypicalHex (const CString& addon);
};

//Bill Lee: never call delete on IantiLeech, use destoryAntiLeechInstat instead.
extern "C" IantiLeech* createAntiLeechInstant();
extern "C" int destoryAntiLeechInstant(IantiLeech*);

#endif
