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

#ifndef KAD2CONSTANTS_H
#define KAD2CONSTANTS_H

// Kad protocol versions, as advertised in the version byte of every Kad2 contact record and
// compared against CContact::GetVersion(). The named constants exist so version gates read as
// protocol requirements rather than magic numbers: a peer at 0x08 must not be sent 0x09 features.
#define KADEMLIA_VERSION1_46c 0x01 /* 45b - 46c */
#define KADEMLIA_VERSION2_47a 0x02 /* 47a */
#define KADEMLIA_VERSION3_47b 0x03 /* 47b */
#define KADEMLIA_VERSION4_47c 0x04 /* 47c */
#define KADEMLIA_VERSION5_48a 0x05 /* -0.48a */
#define KADEMLIA_VERSION6_49aBETA \
	0x06                       /* -0.49aBETA1: OP_FWCHECKUDPREQ, obfuscation, direct callbacks, \
				      source type 6, UDP firewall check */
#define KADEMLIA_VERSION7_49a 0x07 /* -0.49a: OP_KAD_FWTCPCHECK_ACK, KADEMLIA_FIREWALLED2_REQ */
#define KADEMLIA_VERSION8_49b 0x08 /* TAG_KADMISCOPTIONS, KADEMLIA2_HELLO_RES_ACK */
#define KADEMLIA_VERSION9_50a 0x09 /* AICH hashes on keyword storage */

// Our own advertised version. 0x0a alongside the AICH keyword-storage support 0x09 introduced; 0x0a
// adds no further wire element of its own and is the level eMule/eMuleAI advertise.
//
// The compile-time default is 0x08 for maximum backward compatibility. At runtime, the preference
// KadProtocol10 (set in Preferences, exposed over EC and amuleapi) can raise the advertised version
// to 0x0a and enable the AICH keyword-storage features 0x09 introduced.
//
// Note for a future bump: CT_EMULE_MISCOPTIONS2 has to change once the Kad version reaches 0x0F,
// the eD2k capability field reserving only four bits for it.
#define KADEMLIA_VERSION_DEFAULT 0x08 /* 0.49b */
#define KADEMLIA_VERSION_PROTOCOL10 0x0a

#endif // KAD2CONSTANTS_H
