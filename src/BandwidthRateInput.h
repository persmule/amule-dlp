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

#ifndef AMULE_BANDWIDTH_RATE_INPUT_H
#define AMULE_BANDWIDTH_RATE_INPUT_H

#include <cmath>
#include <wx/numformatter.h>
#include <wx/spinctrl.h>

namespace BandwidthRateConversion
{
// Generic spin controls display localized grouping and decimal separators, and
// may not commit their text until focus loss. Never use a failed partial parse.
inline double ReadMegabits(const wxString &text, double fallback)
{
	double value;
	return wxNumberFormatter::FromString(text, &value) && std::isfinite(value) && value >= 0 ? value
												 : fallback;
}
// Share one fallback between previews, spin events, and OK. GetValue() is not a
// safe fallback: GTK can derive zero or a partial number from invalid text.
class MegabitInputState
{
public:
	double Update(double value)
	{
		if (std::isfinite(value) && value >= 0) {
			m_value = value;
			m_validText = true;
		}
		return m_value;
	}

	double Read(const wxString &text)
	{
		const double value = ReadMegabits(text, -1);
		m_validText = value >= 0;
		if (m_validText) {
			m_value = value;
		}
		return m_value;
	}
	double Read(wxSpinCtrlDouble *input) { return Read(input->GetTextValue()); }

	// Native focus loss can first send a spin event with zero or a partial
	// number for invalid text. Restore our fallback before that value is read.
	double Spin(wxSpinCtrlDouble *input, double value)
	{
		return m_validText ? Update(value) : Commit(input);
	}

	double Commit(wxSpinCtrlDouble *input)
	{
		if (m_validText) {
			Read(input);
		}
		input->SetValue(m_value);
		return Read(input);
	}

private:
	double m_value = 0;
	bool m_validText = true;
};
} // namespace BandwidthRateConversion
#endif
