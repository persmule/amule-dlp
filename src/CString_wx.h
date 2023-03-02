/**
 * Author:	Bill Lee<bill.lee.y@gmail.com>
 * License:	GNU GPL
 */
//---------------------
#ifndef CSTRING_WX_H
#define CSTRING_WX_H

//#include <wx/wx.h>
#include <wx/string.h>

#if (wxMAJOR_VERSION <= 2) && (wxMINOR_VERSION <= 8)
#define CSTRING_MAXLEN wxSTRING_MAXLEN
#else
#define CSTRING_MAXLEN wxString::npos
#endif

class CString : public wxString{
	public:
		CString(){}
		CString(wxChar c, size_t n=1): wxString(c, n){}
		CString(const wxChar* str): wxString(str){}
		CString(const wxString& str): wxString(str){}
		CString(const CString& str): wxString(str){}
		//---------------------
		CString& operator=(const wxChar* str){
			wxString::operator=(str);
			return *this;
		}
		//operator*() from wxString;
		size_t GetLength()const{	return Length();	}
		wxChar GetAt(size_t nIndex)const{	return GetChar(nIndex);	}
		//IsEmpty() from wxString;
		CString& TrimLeft(wxChar c){
			size_t pos = find_first_not_of(c);
			if(pos == 0)
				return *this;
			erase(0, pos);
			return *this;
		}
		CString& TrimRight(wxChar c){
			size_t pos = find_last_not_of(c) + 1;
			if(pos == Length())
				return *this;
			erase(pos, Length() - pos);
			return *this;
		}
		CString Trim(){
			CString ret(*this);
			ret.wxString::Trim(false);        /* wxString::Trim(bool fromright = true) */
			ret.wxString::Trim(true);
			return ret;
		}
		//Find(wxChar) and Find(wxChar*) from wxString;
		int Find(const CString& str)const{	return wxString::Find(str.c_str());	}
		int ReverseFind(const wxChar c)const{	return wxString::Find(c, true);	}
		int ReverseFind(const wxChar* str)const{	return rfind(str);	}
		int ReverseFind(const CString& str)const{	return rfind(str);	}
		CString Right(size_t len)const{	return wxString::Right(len);	}
		CString Left(size_t len)const{	return wxString::Left(len);	}
		CString Mid(size_t first, size_t count = CSTRING_MAXLEN)const{
			return wxString::Mid(first, count);
		}
};
#endif
