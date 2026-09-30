//
// This file is part of the aMule Project.
//
// Parts of this file are based on work from pan One (http://home-3.tiscali.nl/~meost/pms/)
//
// Copyright (c) 2003-2026 aMule Team ( https://amule-org.github.io )
// Copyright (c) 2002-2011 Merkur ( devs@emule-project.net / http://www.emule-project.net )
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

#include <algorithm>
#include "KnownFile.h"
#include <common/StringFunctions.h>

/* Abstract File (base class)*/

CAbstractFile::CAbstractFile()
: m_iRating(0)
, m_hasComment(false)
, m_iUserRating(0)
, m_kadCommentSearchRunning(false)
, m_nFileSize(0)
{
}

CAbstractFile::CAbstractFile(const CAbstractFile &other)
: m_abyFileHash(other.m_abyFileHash)
, m_strComment(other.m_strComment)
, m_iRating(other.m_iRating)
, m_hasComment(other.m_hasComment)
, m_iUserRating(other.m_iUserRating)
, m_taglist(other.m_taglist)
, m_kadNotes()
, m_kadCommentSearchRunning(false)
, m_nFileSize(other.m_nFileSize)
, m_fileName(other.m_fileName)
{
	/* // TODO: Currently it's not safe to duplicate the entries, but isn't needed either.
		CKadEntryPtrList::const_iterator it = other.m_kadNotes.begin();
		for (; it != other.m_kadNotes.end(); ++it) {
			m_kadNotes.push_back(new Kademlia::CEntry(**it));
		}
	*/
}

void CAbstractFile::SetFileName(const CPath &fileName)
{
	m_fileName = fileName;
}

uint32 CAbstractFile::GetIntTagValue(uint8 tagname) const
{
	ArrayOfCTag::const_iterator it = m_taglist.begin();
	for (; it != m_taglist.end(); ++it) {
		if (((*it).GetNameID() == tagname) && (*it).IsInt()) {
			return (*it).GetInt();
		}
	}
	return 0;
}

bool CAbstractFile::GetIntTagValue(uint8 tagname, uint32 &ruValue) const
{
	ArrayOfCTag::const_iterator it = m_taglist.begin();
	for (; it != m_taglist.end(); ++it) {
		if (((*it).GetNameID() == tagname) && (*it).IsInt()) {
			ruValue = (*it).GetInt();
			return true;
		}
	}
	return false;
}

uint32 CAbstractFile::GetIntTagValue(const wxString &tagname) const
{
	ArrayOfCTag::const_iterator it = m_taglist.begin();
	for (; it != m_taglist.end(); ++it) {
		if ((*it).IsInt() && ((*it).GetName() == tagname)) {
			return (*it).GetInt();
		}
	}
	return 0;
}

const wxString &CAbstractFile::GetStrTagValue(uint8 tagname) const
{
	ArrayOfCTag::const_iterator it = m_taglist.begin();
	for (; it != m_taglist.end(); ++it) {
		if ((*it).GetNameID() == tagname && (*it).IsStr()) {
			return (*it).GetStr();
		}
	}
	return EmptyString;
}

const wxString &CAbstractFile::GetStrTagValue(const wxString &tagname) const
{
	ArrayOfCTag::const_iterator it = m_taglist.begin();
	for (; it != m_taglist.end(); ++it) {
		if ((*it).IsStr() && ((*it).GetName() == tagname)) {
			return (*it).GetStr();
		}
	}
	return EmptyString;
}

const CTag *CAbstractFile::GetTag(uint8 tagname, uint8 tagtype) const
{
	ArrayOfCTag::const_iterator it = m_taglist.begin();
	for (; it != m_taglist.end(); ++it) {
		if ((*it).GetNameID() == tagname && (*it).GetType() == tagtype) {
			return &(*it);
		}
	}
	return nullptr;
}

const CTag *CAbstractFile::GetTag(const wxString &tagname, uint8 tagtype) const
{
	ArrayOfCTag::const_iterator it = m_taglist.begin();
	for (; it != m_taglist.end(); ++it) {
		if ((*it).GetType() == tagtype && (*it).GetName() == tagname) {
			return &(*it);
		}
	}
	return nullptr;
}

const CTag *CAbstractFile::GetTag(uint8 tagname) const
{
	ArrayOfCTag::const_iterator it = m_taglist.begin();
	for (; it != m_taglist.end(); ++it) {
		if ((*it).GetNameID() == tagname) {
			return &(*it);
		}
	}
	return nullptr;
}

const CTag *CAbstractFile::GetTag(const wxString &tagname) const
{
	ArrayOfCTag::const_iterator it = m_taglist.begin();
	for (; it != m_taglist.end(); ++it) {
		if ((*it).GetName() == tagname) {
			return &(*it);
		}
	}
	return nullptr;
}

void CAbstractFile::AddTagUnique(const CTag &rTag)
{
	ArrayOfCTag::iterator it = m_taglist.begin();
	for (; it != m_taglist.end(); ++it) {
		if ((((*it).GetNameID() != 0 && (*it).GetNameID() == rTag.GetNameID()) ||
			    (!(*it).GetName().IsEmpty() && !rTag.GetName().IsEmpty() &&
				    (*it).GetName() == rTag.GetName())) &&
			(*it).GetType() == rTag.GetType()) {
			it = m_taglist.erase(it);
			m_taglist.insert(it, rTag);
			return;
		}
	}
	m_taglist.push_back(rTag);
}

bool CAbstractFile::RemoveTag(uint8 tagname)
{
	// Matches on the numeric id alone, unlike AddTagUnique's (id, type) pair: the caller wants
	// the field gone whatever width or encoding it was stored with, and a media tag inherited
	// from a search result can arrive as a narrower integer type than a local probe writes.
	//
	// Erases EVERY match, not just the first. AddTagUnique replaces only when the type matches
	// too, so two tags with one id and different types can legitimately coexist.
	const size_t before = m_taglist.size();
	m_taglist.erase(std::remove_if(m_taglist.begin(),
				m_taglist.end(),
				[tagname](const CTag &tag) { return tag.GetNameID() == tagname; }),
		m_taglist.end());
	return m_taglist.size() != before;
}
