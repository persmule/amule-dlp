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

#include <muleunit/test.h>
#include <SHAHashSet.h>
#include <SearchFile.h>
#include <memory>
#include <KadAICHVotes.h>
#include <Preferences.h>
#include <Logger.h>
#include <kademlia/kademlia/AICHHashList.h>

using namespace muleunit;
using Kademlia::CKadAICHHashList;

DECLARE_SIMPLE(AICHSearchResult)

// Ownerless consensus is supported; enable real logging so these tests do not
// rely on MULEUNIT discarding expressions that access the owner.

static CAICHHash MakeRoot(uint8_t seed)
{
	CAICHHash hash;
	memset(hash.GetRawHash(), seed, CAICHHash::GetHashSize());
	return hash;
}

TEST(AICHSearchResult, FabricatedCountsContributeOnlyOneVote)
{
	theLogger.SetVerbose(true);
	for (uint8_t claimed : { 2, 3, 255 }) {
		std::vector<uint8_t> payload(2 + CAICHHash::GetHashSize(), 0xAB);
		payload[0] = 1;
		payload[1] = claimed;
		std::vector<CKadAICHHashList::SResultHash> decoded;
		ASSERT_TRUE(CKadAICHHashList::DecodeResultTag(payload.data(), payload.size(), decoded));
		const auto *candidate = CKadAICHHashList::SelectCandidate(decoded, claimed);
		ASSERT_TRUE(candidate != nullptr);
		CAICHHash root;
		memcpy(root.GetRawHash(), candidate->m_hash.data(), CAICHHash::GetHashSize());
		CAICHHashSet hashes(nullptr);
		// Peer byte order for 1.2.3.4. The Kad call site swaps its address once.
		const uint32_t responder = 0x04030201;
		hashes.SearchResultHashReceived(root, true, responder);
		ASSERT_EQUALS(AICH_UNTRUSTED, hashes.GetStatus());
		for (unsigned i = 0; i < 20; ++i) {
			hashes.SearchResultHashReceived(root, true, responder);
			// Another address in the same /20 is also not an independent vote.
			hashes.UntrustedHashReceived(root, 0x05030201);
		}
		ASSERT_EQUALS(AICH_UNTRUSTED, hashes.GetStatus());
		for (uint32_t i = 2; i <= 9; ++i) {
			hashes.UntrustedHashReceived(root, 0x04030200 | i);
			ASSERT_EQUALS(AICH_UNTRUSTED, hashes.GetStatus());
		}
		hashes.UntrustedHashReceived(root, 0x0403020A);
		ASSERT_EQUALS(AICH_TRUSTED, hashes.GetStatus());
		ASSERT_TRUE(hashes.GetMasterHash() == root);
	}
}

TEST(AICHSearchResult, ConflictingReportsStillRequireAgreement)
{
	CAICHHashSet hashes(nullptr);
	const CAICHHash candidate = MakeRoot(0xAB);
	const CAICHHash other = MakeRoot(0xCD);
	hashes.SearchResultHashReceived(candidate, true, 0x04030201);
	for (uint32_t i = 2; i <= 11; ++i) {
		hashes.UntrustedHashReceived(other, 0x04030200 | i);
	}
	// Ten of eleven is below the existing 92% threshold.
	ASSERT_EQUALS(AICH_UNTRUSTED, hashes.GetStatus());
	hashes.UntrustedHashReceived(other, 0x0403020C);
	ASSERT_EQUALS(AICH_UNTRUSTED, hashes.GetStatus());
	hashes.UntrustedHashReceived(other, 0x0403020D);
	ASSERT_EQUALS(AICH_TRUSTED, hashes.GetStatus());
	ASSERT_TRUE(hashes.GetMasterHash() == other);
}

TEST(AICHSearchResult, MissingProvenanceDoesNotVoteAndServerPolicyIsUnchanged)
{
	const CAICHHash root = MakeRoot(0xAB);
	CAICHHashSet kad(nullptr);
	kad.SearchResultHashReceived(root, true, 0);
	ASSERT_EQUALS(AICH_EMPTY, kad.GetStatus());
	ASSERT_FALSE(kad.HasValidMasterHash());

	CAICHHashSet server(nullptr);
	server.SearchResultHashReceived(root, false, 0);
	ASSERT_EQUALS(AICH_TRUSTED, server.GetStatus());
	ASSERT_TRUE(server.GetMasterHash() == root);
}

TEST(AICHSearchResult, VerifiedRootCannotBeReplacedByKad)
{
	CAICHHashSet hashes(nullptr);
	const CAICHHash verified = MakeRoot(0xAB);
	hashes.SetMasterHash(verified, AICH_VERIFIED);
	hashes.SearchResultHashReceived(MakeRoot(0xCD), true, 0x04030201);
	ASSERT_EQUALS(AICH_VERIFIED, hashes.GetStatus());
	ASSERT_TRUE(hashes.GetMasterHash() == verified);
}

TEST(AICHSearchResult, MergedRespondersReachConsensus)
{
	CKadAICHVotes merged;
	const CAICHHash root = MakeRoot(0xAB);
	for (uint32_t i = 1; i <= 10; ++i) {
		CKadAICHVotes incoming;
		incoming.Add(0x04030200 | i, root);
		merged.Merge(incoming);
	}
	ASSERT_EQUALS(size_t(10), merged.Get().size());
	CAICHHashSet hashes(nullptr);
	for (const auto &vote : merged.Get()) {
		hashes.UntrustedHashReceived(vote.second, vote.first);
	}
	ASSERT_EQUALS(AICH_TRUSTED, hashes.GetStatus());
	ASSERT_TRUE(hashes.GetMasterHash() == root);
}

TEST(AICHSearchResult, MergePreservesDisagreementAndDeduplicatesResponders)
{
	CKadAICHVotes merged;
	const CAICHHash root = MakeRoot(0xAB);
	const CAICHHash other = MakeRoot(0xCD);
	merged.Add(0x04030201, other);
	for (uint32_t i = 1; i <= 11; ++i) {
		CKadAICHVotes incoming;
		incoming.Add(0x04030200 | i, root);
		merged.Merge(incoming);
	}
	ASSERT_EQUALS(size_t(11), merged.Get().size());
	ASSERT_TRUE(merged.Get().at(0x04030201) == other);
	CAICHHashSet hashes(nullptr);
	for (const auto &vote : merged.Get()) {
		hashes.UntrustedHashReceived(vote.second, vote.first);
	}
	ASSERT_EQUALS(AICH_UNTRUSTED, hashes.GetStatus());
}

TEST(AICHSearchResult, VotesAreBoundedAndUnknownRespondersExcluded)
{
	CKadAICHVotes merged;
	const CAICHHash root = MakeRoot(0xAB);
	merged.Add(0, root);
	ASSERT_TRUE(merged.Get().empty());
	for (uint32_t i = 1; i <= 1000; ++i) {
		CKadAICHVotes incoming;
		incoming.Add(i, root);
		merged.Merge(incoming);
	}
	ASSERT_EQUALS(size_t(64), merged.Get().size());
	CKadAICHVotes copy(merged);
	ASSERT_EQUALS(size_t(64), copy.Get().size());
	ASSERT_TRUE(copy.Get().at(1) == root);
}

// Build model rows without consulting the live download/known-file queues.
// Constructors, ownership, copying, AddChild, MergeResults and evidence replay
// are production code; the fixture only supplies incoming search data.
class CSearchFileTestFixture
{
public:
	static CSearchFile *Result(
		const wxString &name, uint32_t responder, const CAICHHash &root, bool kad = true)
	{
		auto *result = new CSearchFile;
		result->SetFileName(CPath(name));
		result->SetFileSize(1024);
		result->m_kademlia = kad;
		result->m_kadAICHVotes.Add(responder, root);
		return result;
	}
};

TEST(AICHSearchResult, EveryFilenameChildReplaysAllGroupVotes)
{
	const CAICHHash root = MakeRoot(0xAB);
	std::unique_ptr<CSearchFile> group(CSearchFileTestFixture::Result("first", 1, root));
	// Exercise the duplicate-name deletion path before a group has children.
	group->AddChild(CSearchFileTestFixture::Result("first", 2, root));
	ASSERT_FALSE(group->HasChildren());
	// Creating the next variant copies the original row into the first child.
	for (uint32_t i = 3; i <= 10; ++i) {
		group->AddChild(CSearchFileTestFixture::Result(i <= 6 ? "second" : "third", i, root));
	}
	ASSERT_EQUALS(size_t(3), group->GetChildren().size());
	ASSERT_EQUALS(size_t(10), group->GetKadAICHVotes().size());
	ASSERT_EQUALS(size_t(2), group->GetChildren().front()->GetKadAICHVotes().size());
	for (const CSearchFile *child : group->GetChildren()) {
		ASSERT_TRUE(child->GetParent() == group.get());
		const CPath filename = child->GetFileName();
		CAICHHashSet downloaded(nullptr);
		// This is the same production handoff called by CPartFile's constructor.
		ASSERT_TRUE(child->ApplyKadAICHVotes(downloaded));
		ASSERT_EQUALS(AICH_TRUSTED, downloaded.GetStatus());
		ASSERT_TRUE(downloaded.GetMasterHash() == root);
		ASSERT_TRUE(child->GetFileName() == filename);
	}
	CAICHHashSet parentDownload(nullptr);
	ASSERT_TRUE(group->ApplyKadAICHVotes(parentDownload));
	ASSERT_EQUALS(AICH_TRUSTED, parentDownload.GetStatus());
}

TEST(AICHSearchResult, ChildDownloadRetainsOtherVariantsDisagreement)
{
	const CAICHHash root = MakeRoot(0xAB);
	const CAICHHash other = MakeRoot(0xCD);
	std::unique_ptr<CSearchFile> group(CSearchFileTestFixture::Result("minority", 1, other));
	for (uint32_t i = 2; i <= 11; ++i) {
		group->AddChild(CSearchFileTestFixture::Result("majority", i, root));
	}
	ASSERT_EQUALS(size_t(2), group->GetChildren().size());
	const CSearchFile *majority = group->GetChildren().back();
	ASSERT_EQUALS(size_t(10), majority->GetKadAICHVotes().size());
	CAICHHashSet downloaded(nullptr);
	ASSERT_TRUE(majority->ApplyKadAICHVotes(downloaded));
	// Its own ten matching votes would be trusted. Including the other filename's
	// disagreement keeps ten out of eleven below the existing 92% threshold.
	ASSERT_EQUALS(AICH_UNTRUSTED, downloaded.GetStatus());
}

TEST(AICHSearchResult, ServerFirstAllSearchGroupReplaysKadVotes)
{
	const CAICHHash root = MakeRoot(0xAB);
	std::unique_ptr<CSearchFile> group(CSearchFileTestFixture::Result("server", 0, root, false));
	for (uint32_t i = 1; i <= 10; ++i) {
		group->AddChild(CSearchFileTestFixture::Result("kad", i, root));
	}
	ASSERT_FALSE(group->IsKademlia());
	ASSERT_EQUALS(size_t(10), group->GetKadAICHVotes().size());
	for (const CSearchFile *child : group->GetChildren()) {
		CAICHHashSet downloaded(nullptr);
		ASSERT_TRUE(child->ApplyKadAICHVotes(downloaded));
		ASSERT_EQUALS(AICH_TRUSTED, downloaded.GetStatus());
		ASSERT_TRUE(downloaded.GetMasterHash() == root);
	}
}
