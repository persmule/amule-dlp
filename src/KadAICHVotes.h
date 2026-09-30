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

#ifndef KADAICHVOTES_H
#define KADAICHVOTES_H

#include <map>
#include "SHAHashSet.h"

// Transient search-result evidence, never persisted as trusted metadata.
class CKadAICHVotes
{
public:
	void Add(uint32_t responder, const CAICHHash &root)
	{
		// Retain the first root per responder and bound memory per result.
		if (responder != 0 && m_votes.size() < 64) {
			m_votes.emplace(responder, root);
		}
	}

	void Merge(const CKadAICHVotes &other)
	{
		for (const auto &vote : other.m_votes) {
			Add(vote.first, vote.second);
		}
	}

	const std::map<uint32_t, CAICHHash> &Get() const { return m_votes; }

private:
	std::map<uint32_t, CAICHHash> m_votes;
};

#endif
