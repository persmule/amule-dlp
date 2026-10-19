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

#include "SearchColumnFit.h"

#include <algorithm>
#include <vector>

void SampleSearchColumnFitRows(const wxDataViewCtrl &view, wxDataViewItemArray &rows, size_t limit)
{
	rows.Clear();
	const wxDataViewModel *model = view.GetModel();
	if (!model || limit == 0) {
		return;
	}
	wxDataViewItemArray roots;
	model->GetChildren(wxDataViewItem(), roots);
	if (roots.empty()) {
		return;
	}
	const auto contains = [](const wxDataViewItemArray &items, const wxDataViewItem &item) {
		return std::find(items.begin(), items.end(), item) != items.end();
	};
	const auto append = [&](const wxDataViewItem &item) {
		if (!contains(rows, item)) {
			rows.Add(item);
		}
	};

	// A current item can survive collapsing an ancestor. Only keep it if it
	// is still in the filtered, expanded tree being fitted.
	const wxDataViewItem current = view.GetCurrentItem();
	if (current.IsOk()) {
		wxDataViewItem node = current;
		bool visible = true;
		for (wxDataViewItem parent = model->GetParent(node); parent.IsOk();
			node = parent, parent = model->GetParent(node)) {
			wxDataViewItemArray siblings;
			model->GetChildren(parent, siblings);
			if (!view.IsExpanded(parent) || !contains(siblings, node)) {
				visible = false;
				break;
			}
		}
		if (visible && contains(roots, node)) {
			append(current);
		}
	}
	if (rows.size() == limit) {
		return;
	}

	// Reserve room for descendants, but sample roots across their entire
	// range first. An early large group must not hide the last roots.
	const size_t budget = limit - rows.size();
	size_t rootCount = std::min(roots.size(), std::max<size_t>(1, budget / 2));
	bool expanded = false;
	// Probe only the strided roots we would sample. Asking IsExpanded() for
	// every stored root can itself be expensive on native tree backends.
	for (size_t i = 0; i < rootCount; ++i) {
		const size_t index = rootCount > 1 ? i * (roots.size() - 1) / (rootCount - 1) : 0;
		if (view.IsExpanded(roots[index])) {
			expanded = true;
			break;
		}
	}
	if (!expanded) {
		rootCount = std::min(roots.size(), budget);
	}
	std::vector<wxDataViewItem> pending;
	const auto sample = [&](const wxDataViewItemArray &items, size_t count) {
		for (size_t i = 0; i < count; ++i) {
			const size_t index = count > 1 ? i * (items.size() - 1) / (count - 1) : 0;
			const wxDataViewItem item = items[index];
			append(item);
			if (view.IsExpanded(item)) {
				pending.push_back(item);
			}
		}
	};
	sample(roots, rootCount);
	// Breadth first, sharing the remaining budget across expanded siblings.
	// Each GetChildren() already applies the model's visibility rules,
	// including matching variants and browse folders.
	for (size_t i = 0; i < pending.size() && rows.size() < limit; ++i) {
		wxDataViewItemArray children;
		model->GetChildren(pending[i], children);
		const size_t quota = std::max<size_t>(1, (limit - rows.size()) / (pending.size() - i));
		sample(children, std::min(children.size(), quota));
	}
}
