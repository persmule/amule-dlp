//
// This file is part of the aMule Project.
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

#ifndef SEARCHSOURCEFORMAT_H
#define SEARCHSOURCEFORMAT_H

#include "SearchSourceCount.h"
#include <common/Format.h>
#include <optional>
#include <wx/intl.h>

// Compact cell text; detailed counts belong in the tooltip.
inline wxString FormatSearchSources(
	uint32_t total, const std::optional<CSearchSourceCount> &networks = std::nullopt)
{
	wxString text;
	if (networks) {
		if (networks->Ed2k()) {
			text = CFormat("E:%u") % networks->Ed2k();
		}
		if (networks->Kad()) {
			if (!text.empty()) {
				text += " ";
			}
			text += CFormat("K:%u") % networks->Kad();
		}
	}
	return text.empty() ? (CFormat("%u") % total).GetString() : text;
}

inline wxString FormatSearchSourcesTooltip(uint32_t total,
	uint32_t complete,
	std::optional<size_t> clients,
	const std::optional<CSearchSourceCount> &networks = std::nullopt)
{
	wxString text = CFormat(_("Estimated availability: %u")) % total;
	if (networks) {
		if (networks->Ed2k()) {
			text += "\n" + (CFormat(_("eD2k sources: %u")) % networks->Ed2k()).GetString();
		}
		if (networks->Kad()) {
			text += "\n" + (CFormat(_("Kad sources: %u")) % networks->Kad()).GetString();
		}
		if (networks->Ed2k() && networks->Kad()) {
			text += "\n" + _("Network counts may overlap; they are not added together.");
		}
	} else {
		text += "\n" + _("Network breakdown unavailable.");
	}
	text += "\n" + (CFormat(_("Complete sources: %u")) % complete).GetString();
	if (clients) {
		text += "\n" + (CFormat(_("Direct client endpoints: %u")) % *clients).GetString();
	}
	return text;
}

#endif
