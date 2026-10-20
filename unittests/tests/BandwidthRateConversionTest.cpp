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

#include <muleunit/test.h>
#include "BandwidthRateConversion.h"
#include <limits>
using namespace muleunit;
DECLARE_SIMPLE(BandwidthRateConversion)
TEST(BandwidthRateConversion, MegabitConversion)
{
	ASSERT_EQUALS(0, BandwidthRateConversion::FromMegabits(0, 1000000));
	ASSERT_EQUALS(12500, BandwidthRateConversion::FromMegabits(102.4, 1000000));
	ASSERT_EQUALS(1, BandwidthRateConversion::FromMegabits(0.0001, 1000000));
	ASSERT_EQUALS(1000000, BandwidthRateConversion::FromMegabits(8192, 1000000));
	ASSERT_EQUALS(1000000, BandwidthRateConversion::FromMegabits(100000, 1000000));
	ASSERT_EQUALS(0, BandwidthRateConversion::FromMegabits(-1, 1000000));
	ASSERT_EQUALS(
		0, BandwidthRateConversion::FromMegabits(std::numeric_limits<double>::quiet_NaN(), 1000000));
	ASSERT_EQUALS(
		0, BandwidthRateConversion::FromMegabits(std::numeric_limits<double>::infinity(), 1000000));
	for (int rate = 0; rate <= 1000000; ++rate) {
		ASSERT_EQUALS(rate,
			BandwidthRateConversion::FromMegabits(
				BandwidthRateConversion::ToMegabits(rate), 1000000));
	}
}

TEST(BandwidthRateConversion, ControlRange)
{
	ASSERT_EQUALS(500, BandwidthRateConversion::FromMegabits(8192, 500));
	ASSERT_EQUALS(2000000, BandwidthRateConversion::FromMegabits(16384, 2000000));
	ASSERT_EQUALS(2000000, BandwidthRateConversion::FromMegabits(100000, 2000000));
}
