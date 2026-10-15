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

#include <zlib.h> // Needed for uLongf

#include "Packet.h" // Interface declarations

#include <protocol/Protocols.h>

#include "Logger.h"       // Needed for AddDebugLogLineN
#include "MemFile.h"      // Needed for CMemFile
#include "OtherStructs.h" // Needed for Header_Struct
#include "ArchSpecific.h" // Needed for ENDIAN_*
#include <common/MuleDebug.h>
#include <algorithm>
#include <memory>

namespace
{
// Match the legacy receive-header limit, leaving room for allocation overhead.
constexpr uint32 MaxPacketPayload = 0x7fffffeeu;

uint32 CheckedPayloadSize(uint64 bytes)
{
	MULE_VALIDATE_PARAMS(bytes <= MaxPacketPayload, "Packet payload is too large.");
	return static_cast<uint32>(bytes);
}
} // namespace

// Copy constructor
CPacket::CPacket(CPacket &p)
{
	size = p.size;
	opcode = p.opcode;
	prot = p.prot;
	m_bSplitted = p.m_bSplitted;
	m_bLastSplitted = p.m_bLastSplitted;
	m_bPacked = p.m_bPacked;
	m_bFromPF = p.m_bFromPF;
	memcpy(head, p.head, sizeof head);
	tempbuffer = NULL;
	if (p.completebuffer) {
		completebuffer = new uint8_t[static_cast<size_t>(size) + 10];
		pBuffer = completebuffer + sizeof(Header_Struct);
	} else {
		completebuffer = NULL;
		if (p.pBuffer) {
			pBuffer = new uint8_t[size];
		} else {
			pBuffer = NULL;
		}
	}
	if (pBuffer)
		memcpy(pBuffer, p.pBuffer, size);
}

CPacket::CPacket(uint8 protocol)
{
	size = 0;
	opcode = 0;
	prot = protocol;
	m_bSplitted = false;
	m_bLastSplitted = false;
	m_bPacked = false;
	m_bFromPF = false;
	memset(head, 0, sizeof head);
	tempbuffer = NULL;
	completebuffer = NULL;
	pBuffer = NULL;
}

// only used for receiving packets
CPacket::CPacket(uint8_t *rawHeader, uint8_t *buf)
{
	memset(head, 0, sizeof head);
	Header_Struct *header = reinterpret_cast<Header_Struct *>(rawHeader);
	const uint32 length = ENDIAN_SWAP_32(header->packetlength);
	MULE_CHECK_THROW(
		length >= 1 && length - 1 <= MaxPacketPayload, CInvalidPacket, "Invalid packet length.");
	size = length - 1;
	opcode = header->command;
	prot = header->eDonkeyID;
	m_bSplitted = false;
	m_bLastSplitted = false;
	m_bPacked = false;
	m_bFromPF = false;
	tempbuffer = NULL;
	completebuffer = NULL;
	pBuffer = buf;
}

CPacket::CPacket(const CMemFile &datafile, uint8 protocol, uint8 ucOpcode)
{
	size = CheckedPayloadSize(datafile.GetLength());
	opcode = ucOpcode;
	prot = protocol;
	m_bSplitted = false;
	m_bLastSplitted = false;
	m_bPacked = false;
	m_bFromPF = false;
	memset(head, 0, sizeof head);
	tempbuffer = NULL;
	completebuffer = new uint8_t[size + sizeof(Header_Struct) /*Why this 4?*/];
	pBuffer = completebuffer + sizeof(Header_Struct);

	// Write contents of MemFile to buffer (while keeping original position in file)
	const uint64 position = datafile.GetPosition();
	datafile.Seek(0, wxFromStart);
	datafile.Read(pBuffer, size);
	datafile.Seek(position, wxFromStart);
}

CPacket::CPacket(int8 in_opcode, uint64 in_size, uint8 protocol, bool bFromPF)
{
	size = CheckedPayloadSize(in_size);
	opcode = in_opcode;
	prot = protocol;
	m_bSplitted = false;
	m_bLastSplitted = false;
	m_bPacked = false;
	m_bFromPF = bFromPF;
	memset(head, 0, sizeof head);
	tempbuffer = NULL;
	if (size) {
		completebuffer = new uint8_t[static_cast<size_t>(size) + sizeof(Header_Struct) + 4];
		pBuffer = completebuffer + sizeof(Header_Struct);
		memset(completebuffer, 0, static_cast<size_t>(size) + sizeof(Header_Struct) + 4);
	} else {
		completebuffer = NULL;
		pBuffer = NULL;
	}
}

// only used for splitted packets!
CPacket::CPacket(uint8_t *pPacketPart, uint64 nSize, bool bLast, bool bFromPF)
{
	std::unique_ptr<uint8_t[]> ownedPacketPart(pPacketPart);
	MULE_VALIDATE_PARAMS(nSize >= sizeof(Header_Struct), "Split packet is shorter than its header.");
	size = CheckedPayloadSize(nSize - sizeof(Header_Struct));
	opcode = 0;
	prot = 0;
	m_bSplitted = true;
	m_bLastSplitted = bLast;
	m_bPacked = false;
	m_bFromPF = bFromPF;
	memset(head, 0, sizeof head);
	tempbuffer = NULL;
	completebuffer = ownedPacketPart.release();
	pBuffer = NULL;
}

CPacket::~CPacket()
{
	// Never deletes pBuffer when completebuffer is not NULL. Not a redundant null-guard: pBuffer
	// aliases into completebuffer when that is set, so exactly one of the two owns the allocation.
	// NOLINTNEXTLINE(readability-delete-null-pointer)
	if (completebuffer) {
		delete[] completebuffer;
	} else {
		// If completebuffer is NULL, pBuffer is ours to free (delete[] on a
		// null pBuffer is a no-op).
		delete[] pBuffer;
	}

	delete[] tempbuffer;
}

uint32 CPacket::GetPacketSizeFromHeader(const uint8_t *rawHeader)
{
	const Header_Struct *header = reinterpret_cast<const Header_Struct *>(rawHeader);
	uint32 size = ENDIAN_SWAP_32(header->packetlength);
	if (size < 1 || size - 1 > MaxPacketPayload)
		return InvalidPacketSize;
	return size - 1;
}

void CPacket::CopyToDataBuffer(unsigned int offset, const uint8_t *data, unsigned int n)
{
	wxCHECK_RET(offset <= size && n <= size - offset, "Bad span in CopyToDataBuffer.");
	if (n) {
		wxCHECK_RET(data && pBuffer, "Invalid buffer in CopyToDataBuffer.");
		memcpy(pBuffer + offset, data, n);
	}
}

uint8_t *CPacket::GetPacket()
{
	if (completebuffer) {
		if (!m_bSplitted) {
			memcpy(completebuffer, GetHeader(), sizeof(Header_Struct));
		}
		return completebuffer;
	} else {
		if (tempbuffer) {
			delete[] tempbuffer;
			tempbuffer = NULL;
		}
		tempbuffer = new uint8_t[size + sizeof(Header_Struct) + 4 /* why this 4?*/];
		memcpy(tempbuffer, GetHeader(), sizeof(Header_Struct));
		memcpy(tempbuffer + sizeof(Header_Struct), pBuffer, size);
		return tempbuffer;
	}
}

uint8_t *CPacket::DetachPacket()
{
	if (completebuffer) {
		if (!m_bSplitted) {
			memcpy(completebuffer, GetHeader(), sizeof(Header_Struct));
		}
		uint8_t *result = completebuffer;
		completebuffer = pBuffer = NULL;
		return result;
	} else {
		if (tempbuffer) {
			delete[] tempbuffer;
			tempbuffer = NULL;
		}
		tempbuffer = new uint8_t[size + sizeof(Header_Struct) + 4 /* Why this 4?*/];
		memcpy(tempbuffer, GetHeader(), sizeof(Header_Struct));
		memcpy(tempbuffer + sizeof(Header_Struct), pBuffer, size);
		uint8_t *result = tempbuffer;
		tempbuffer = 0;
		return result;
	}
}

uint8_t *CPacket::GetHeader()
{
	wxASSERT(!m_bSplitted);

	Header_Struct *header = reinterpret_cast<Header_Struct *>(head);
	header->command = opcode;
	header->eDonkeyID = prot;
	header->packetlength = ENDIAN_SWAP_32(size + 1);

	return head;
}

uint8_t *CPacket::GetUDPHeader()
{
	wxASSERT(!m_bSplitted);

	memset(head, 0, 6);
	UDP_Header_Struct *header = reinterpret_cast<UDP_Header_Struct *>(head);
	header->eDonkeyID = prot;
	header->command = opcode;

	return head;
}

void CPacket::PackPacket()
{
	wxASSERT(!m_bSplitted);

	uLongf newsize = static_cast<uLongf>(size) + 300;
	uint8_t *output = new uint8_t[newsize];

	uint16 result = compress2(output, &newsize, pBuffer, size, Z_BEST_COMPRESSION);

	if (result != Z_OK || size <= newsize) {
		delete[] output;
		return;
	}

	if (prot == OP_KADEMLIAHEADER) {
		prot = OP_KADEMLIAPACKEDPROT;
	} else {
		prot = OP_PACKEDPROT;
	}

	memcpy(pBuffer, output, newsize);
	delete[] output;
	m_bPacked = true;

	size = newsize;
}

bool CPacket::UnPackPacket(uint32 uMaxDecompressedSize)
{
	wxASSERT(prot == OP_PACKEDPROT || prot == OP_ED2KV2PACKEDPROT);
	// OP_ED2KV2PACKEDPROT is experimental aMule test code,
	// this should not happen yet. Leave a warning in the log.
	if (prot == OP_ED2KV2PACKEDPROT) {
		AddDebugLogLineN(logPacketErrors, "Received OP_ED2KV2PACKEDPROT.");
	}

	const uint64 estimate = static_cast<uint64>(size) * 10 + 300;
	uint32 nNewSize = static_cast<uint32>(std::min<uint64>(estimate, uMaxDecompressedSize));

	uint8_t *unpack = new uint8_t[nNewSize];
	uLongf unpackedsize = nNewSize;
	int result = uncompress(unpack, &unpackedsize, pBuffer, size);
	if (result == Z_BUF_ERROR && nNewSize < uMaxDecompressedSize) {
		// Packed better than the 10x estimate: retry at the caller's limit.
		delete[] unpack;
		nNewSize = uMaxDecompressedSize;
		unpack = new uint8_t[nNewSize];
		unpackedsize = nNewSize;
		result = uncompress(unpack, &unpackedsize, pBuffer, size);
	}

	if (result == Z_OK) {
		wxASSERT(completebuffer == NULL);
		wxASSERT(pBuffer != NULL);

		size = unpackedsize;
		delete[] pBuffer;
		pBuffer = unpack;
		prot = OP_EMULEPROT;
		return true;
	}

	delete[] unpack;
	return false;
}

void CPacket::Copy16ToDataBuffer(const void *data)
{
	CopyToDataBuffer(0, static_cast<const uint8_t *>(data), 16);
}

void CPacket::CopyUInt32ToDataBuffer(uint32 data, unsigned int offset)
{
	wxCHECK_RET(pBuffer && size >= sizeof(uint32) && offset <= size - sizeof(uint32),
		"Bad offset in CopyUInt32ToDataBuffer.");
	PokeUInt32(pBuffer + offset, data);
}
// File_checked_for_headers
