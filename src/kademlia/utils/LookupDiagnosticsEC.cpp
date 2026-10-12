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

#include "LookupDiagnosticsEC.h"
#include "../../libs/ec/cpp/ECPacket.h"
#include <algorithm>
#include <initializer_list>
#include <limits>
using namespace Kademlia;

void Kademlia::AddLookupDiagnosticsTags(CECPacket &packet, const LookupReports &reports)
{
	for (const auto &snapshot : reports) {
		CECEmptyTag lookup(EC_TAG_KAD_LOOKUP);
		lookup.AddTag(CECTag(EC_TAG_KAD_LOOKUP_TARGET, snapshot.target));
		lookup.AddTag(CECTag(EC_TAG_KAD_LOOKUP_TYPE, static_cast<uint32_t>(snapshot.type)));
		lookup.AddTag(CECTag(EC_TAG_KAD_LOOKUP_KEYWORD, snapshot.keyword));
		lookup.AddTag(CECTag(EC_TAG_KAD_LOOKUP_ACTIVE, uint8_t(snapshot.active)));
		lookup.AddTag(CECTag(EC_TAG_KAD_LOOKUP_OVERDUE, snapshot.overdue));
		lookup.AddTag(CECTag(EC_TAG_KAD_LOOKUP_OMITTED_PEERS, snapshot.omittedPeers));
		lookup.AddTag(CECTag(EC_TAG_KAD_LOOKUP_EVICTED_REFERRALS, snapshot.evictedReferrals));
		for (const auto &item : snapshot.peers) {
			CECEmptyTag peer(EC_TAG_KAD_LOOKUP_PEER);
			peer.AddTag(CECTag(EC_TAG_KAD_LOOKUP_PEER_IP, item.address.first));
			peer.AddTag(CECTag(EC_TAG_KAD_LOOKUP_PEER_PORT, item.address.second));
			peer.AddTag(CECTag(EC_TAG_KAD_LOOKUP_PEER_ID, item.id.size(), item.id.data()));
			peer.AddTag(CECTag(
				EC_TAG_KAD_LOOKUP_PEER_DISTANCE, item.distance.size(), item.distance.data()));
			peer.AddTag(CECTag(EC_TAG_KAD_LOOKUP_PEER_VERSION, item.kadVersion));
			peer.AddTag(CECTag(EC_TAG_KAD_LOOKUP_PEER_REQUESTS, item.requests));
			peer.AddTag(CECTag(EC_TAG_KAD_LOOKUP_PEER_REPLIES, item.replies));
			peer.AddTag(CECTag(EC_TAG_KAD_LOOKUP_PEER_RTT, item.roundTrip));
			peer.AddTag(CECTag(EC_TAG_KAD_LOOKUP_PEER_ITEM_REQUESTS, item.itemRequests));
			peer.AddTag(CECTag(EC_TAG_KAD_LOOKUP_PEER_ITEM_REPLIES, item.itemReplies));
			peer.AddTag(CECTag(EC_TAG_KAD_LOOKUP_PEER_RESULTS, item.results));
			peer.AddTag(CECTag(EC_TAG_KAD_LOOKUP_PEER_PENDING, uint8_t(item.pending)));
			lookup.AddTag(peer);
		}
		for (const auto &event : snapshot.referrals) {
			CECEmptyTag referral(EC_TAG_KAD_LOOKUP_REFERRAL);
			referral.AddTag(CECTag(EC_TAG_KAD_LOOKUP_REFERRAL_SOURCE_IP, event.source.first));
			referral.AddTag(CECTag(EC_TAG_KAD_LOOKUP_REFERRAL_SOURCE_PORT, event.source.second));
			referral.AddTag(CECTag(EC_TAG_KAD_LOOKUP_REFERRAL_PEER_IP, event.peer.first));
			referral.AddTag(CECTag(EC_TAG_KAD_LOOKUP_REFERRAL_PEER_PORT, event.peer.second));
			referral.AddTag(CECTag(EC_TAG_KAD_LOOKUP_REFERRAL_ELAPSED, event.elapsed));
			referral.AddTag(CECTag(EC_TAG_KAD_LOOKUP_REFERRAL_DISTANCE,
				event.distance.size(),
				event.distance.data()));
			referral.AddTag(CECTag(EC_TAG_KAD_LOOKUP_REFERRAL_CLOSER, uint8_t(event.closer)));
			lookup.AddTag(referral);
		}
		packet.AddTag(lookup);
	}
}

namespace
{
uint64_t Number(const CECTag &parent, ec_tagname_t name)
{
	const auto *tag = parent.GetTagByName(name);
	return tag && tag->IsInt() ? tag->GetInt() : 0;
}

struct IntegerField
{
	ec_tagname_t name;
	uint64_t maximum = std::numeric_limits<uint32_t>::max();
};

bool ValidInts(const CECTag &parent, std::initializer_list<IntegerField> fields)
{
	for (const auto &field : fields) {
		const auto *tag = parent.GetTagByName(field.name);
		if (!tag || !tag->IsInt() || tag->GetInt() > field.maximum) {
			return false;
		}
	}
	return true;
}

bool ValidID(const CECTag &parent, ec_tagname_t name)
{
	const auto *tag = parent.GetTagByName(name);
	return tag && tag->IsCustom() && tag->GetTagDataLen() == 16;
}

CLookupTrace::ID ID(const CECTag &parent, ec_tagname_t name)
{
	CLookupTrace::ID id{};
	const auto *tag = parent.GetTagByName(name);
	std::copy_n(static_cast<const uint8_t *>(tag->GetTagData()), id.size(), id.begin());
	return id;
}
} // namespace

bool Kademlia::DecodeLookupDiagnosticsTags(const CECPacket &packet, LookupReports &reports)
{
	reports.clear();
	if (packet.GetOpCode() != EC_OP_GET_KAD_LOOKUPS) {
		return false;
	}
	LookupReports decoded;
	for (const auto &lookup : packet) {
		if (lookup.GetTagName() != EC_TAG_KAD_LOOKUP) {
			return false;
		}
		const auto *target = lookup.GetTagByName(EC_TAG_KAD_LOOKUP_TARGET);
		const auto *type = lookup.GetTagByName(EC_TAG_KAD_LOOKUP_TYPE);
		const auto *keywordTag = lookup.GetTagByName(EC_TAG_KAD_LOOKUP_KEYWORD);
		if (!target || !target->IsString() || !keywordTag || !keywordTag->IsString() || !type ||
			!type->IsInt() || type->GetInt() > std::numeric_limits<uint32_t>::max() ||
			!ValidInts(lookup,
				{ { EC_TAG_KAD_LOOKUP_ACTIVE, 1 },
					{ EC_TAG_KAD_LOOKUP_OVERDUE },
					{ EC_TAG_KAD_LOOKUP_OMITTED_PEERS },
					{ EC_TAG_KAD_LOOKUP_EVICTED_REFERRALS } })) {
			return false;
		}
		for (const auto &child : lookup) {
			if (child.GetTagName() == EC_TAG_KAD_LOOKUP_PEER) {
				if (!ValidInts(child,
					    { { EC_TAG_KAD_LOOKUP_PEER_IP },
						    { EC_TAG_KAD_LOOKUP_PEER_PORT,
							    std::numeric_limits<uint16_t>::max() },
						    { EC_TAG_KAD_LOOKUP_PEER_VERSION,
							    std::numeric_limits<uint8_t>::max() },
						    { EC_TAG_KAD_LOOKUP_PEER_REQUESTS },
						    { EC_TAG_KAD_LOOKUP_PEER_REPLIES },
						    { EC_TAG_KAD_LOOKUP_PEER_RTT,
							    std::numeric_limits<uint64_t>::max() },
						    { EC_TAG_KAD_LOOKUP_PEER_ITEM_REQUESTS },
						    { EC_TAG_KAD_LOOKUP_PEER_ITEM_REPLIES },
						    { EC_TAG_KAD_LOOKUP_PEER_RESULTS },
						    { EC_TAG_KAD_LOOKUP_PEER_PENDING, 1 } }) ||
					!ValidID(child, EC_TAG_KAD_LOOKUP_PEER_ID) ||
					!ValidID(child, EC_TAG_KAD_LOOKUP_PEER_DISTANCE)) {
					return false;
				}
			} else if (child.GetTagName() == EC_TAG_KAD_LOOKUP_REFERRAL &&
				   (!ValidInts(child,
					    { { EC_TAG_KAD_LOOKUP_REFERRAL_SOURCE_IP },
						    { EC_TAG_KAD_LOOKUP_REFERRAL_SOURCE_PORT,
							    std::numeric_limits<uint16_t>::max() },
						    { EC_TAG_KAD_LOOKUP_REFERRAL_PEER_IP },
						    { EC_TAG_KAD_LOOKUP_REFERRAL_PEER_PORT,
							    std::numeric_limits<uint16_t>::max() },
						    { EC_TAG_KAD_LOOKUP_REFERRAL_ELAPSED,
							    std::numeric_limits<uint64_t>::max() },
						    { EC_TAG_KAD_LOOKUP_REFERRAL_CLOSER, 1 } }) ||
					   !ValidID(child, EC_TAG_KAD_LOOKUP_REFERRAL_DISTANCE))) {
				return false;
			}
		}
		LookupReport report;
		report.target = target->GetStringData();
		report.keyword = keywordTag->GetStringData();
		report.type = static_cast<LookupType>(type->GetInt());
		report.active = Number(lookup, EC_TAG_KAD_LOOKUP_ACTIVE) != 0;
		report.overdue = Number(lookup, EC_TAG_KAD_LOOKUP_OVERDUE);
		report.omittedPeers = Number(lookup, EC_TAG_KAD_LOOKUP_OMITTED_PEERS);
		report.evictedReferrals = Number(lookup, EC_TAG_KAD_LOOKUP_EVICTED_REFERRALS);
		for (const auto &child : lookup) {
			if (child.GetTagName() == EC_TAG_KAD_LOOKUP_PEER) {
				LookupPeer peer;
				peer.address.first = Number(child, EC_TAG_KAD_LOOKUP_PEER_IP);
				peer.address.second = Number(child, EC_TAG_KAD_LOOKUP_PEER_PORT);
				peer.kadVersion = Number(child, EC_TAG_KAD_LOOKUP_PEER_VERSION);
				peer.requests = Number(child, EC_TAG_KAD_LOOKUP_PEER_REQUESTS);
				peer.replies = Number(child, EC_TAG_KAD_LOOKUP_PEER_REPLIES);
				peer.roundTrip = Number(child, EC_TAG_KAD_LOOKUP_PEER_RTT);
				peer.itemRequests = Number(child, EC_TAG_KAD_LOOKUP_PEER_ITEM_REQUESTS);
				peer.itemReplies = Number(child, EC_TAG_KAD_LOOKUP_PEER_ITEM_REPLIES);
				peer.results = Number(child, EC_TAG_KAD_LOOKUP_PEER_RESULTS);
				peer.pending = Number(child, EC_TAG_KAD_LOOKUP_PEER_PENDING);
				peer.id = ID(child, EC_TAG_KAD_LOOKUP_PEER_ID);
				peer.distance = ID(child, EC_TAG_KAD_LOOKUP_PEER_DISTANCE);
				report.peers.push_back(peer);
			} else if (child.GetTagName() == EC_TAG_KAD_LOOKUP_REFERRAL) {
				LookupReferral referral;
				referral.source.first = Number(child, EC_TAG_KAD_LOOKUP_REFERRAL_SOURCE_IP);
				referral.source.second =
					Number(child, EC_TAG_KAD_LOOKUP_REFERRAL_SOURCE_PORT);
				referral.peer.first = Number(child, EC_TAG_KAD_LOOKUP_REFERRAL_PEER_IP);
				referral.peer.second = Number(child, EC_TAG_KAD_LOOKUP_REFERRAL_PEER_PORT);
				referral.elapsed = Number(child, EC_TAG_KAD_LOOKUP_REFERRAL_ELAPSED);
				referral.closer = Number(child, EC_TAG_KAD_LOOKUP_REFERRAL_CLOSER);
				referral.distance = ID(child, EC_TAG_KAD_LOOKUP_REFERRAL_DISTANCE);
				report.referrals.push_back(referral);
			}
		}
		decoded.push_back(std::move(report));
	}
	reports = std::move(decoded);
	return true;
}
