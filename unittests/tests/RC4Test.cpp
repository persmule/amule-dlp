// This file is part of the aMule Project.
// Copyright (c) 2026 aMule Team. GPL-2.0-or-later.

#include <muleunit/test.h>
#include <RC4Encrypt.h>
#include <common/MD5Sum.h>
#include "WarningsPush_CryptoPP.h"
#include CRYPTO_HEADER(arc4.h)
#include "WarningsPop.h"

#include <array>
#include <vector>

using namespace muleunit;

DECLARE_SIMPLE(RC4)

TEST(RC4, KnownAnswersWithAndWithoutDiscard)
{
	// A default MD5Sum holds sixteen zero bytes. Constants were generated
	// independently and are also checked against Crypto++ below.
	const MD5Sum key;
	// clang-format off
	const uint8 expected[2][16] = {
		{ 0xde, 0x18, 0x89, 0x41, 0xa3, 0x37, 0x5d, 0x3a,
		  0x8a, 0x06, 0x1e, 0x67, 0x57, 0x6e, 0x92, 0x6d },
		{ 0x9a, 0x88, 0x98, 0xeb, 0xed, 0x25, 0x89, 0x53,
		  0x36, 0xae, 0x11, 0x9a, 0xe5, 0x03, 0xca, 0xd6 }
	};
	// clang-format on
	for (unsigned discard = 0; discard < 2; ++discard) {
		CRC4EncryptableBuffer cipher;
		cipher.SetKey(key, discard == 0);
		std::array<uint8, 16> output{};
		CryptoPP::Weak::ARC4 reference(key.GetRawHash(), 16);
		if (discard) {
			reference.DiscardBytes(1024);
		}
		std::array<uint8, 16> independent{};
		reference.GenerateBlock(independent.data(), independent.size());
		cipher.RC4Crypt(output.data(), output.data(), output.size());
		for (size_t i = 0; i < output.size(); ++i) {
			ASSERT_EQUALS(unsigned(expected[discard][i]), unsigned(output[i]));
			ASSERT_EQUALS(unsigned(expected[discard][i]), unsigned(independent[i]));
		}
	}
}

TEST(RC4, MatchesIndependentCipherAcrossKeysAndOffsets)
{
	for (unsigned seed = 0; seed < 16; ++seed) {
		std::array<uint8, 16> material{};
		for (size_t i = 0; i < material.size(); ++i) {
			material[i] = static_cast<uint8>(seed * 17 + i);
		}
		const MD5Sum key(material.data(), material.size());
		for (bool skipDiscard : { false, true }) {
			CRC4EncryptableBuffer cipher;
			cipher.SetKey(key, skipDiscard);
			CryptoPP::Weak::ARC4 reference(key.GetRawHash(), 16);
			if (!skipDiscard) {
				reference.DiscardBytes(1024);
			}
			for (uint32 length : { 1u, 4u, 255u, 256u, 257u, 1300u, 10240u }) {
				std::vector<uint8> input(length, static_cast<uint8>(seed));
				std::vector<uint8> expected(length), actual(length);
				reference.ProcessData(expected.data(), input.data(), length);
				cipher.RC4Crypt(input.data(), actual.data(), length);
				for (size_t i = 0; i < length; ++i) {
					ASSERT_EQUALS(unsigned(expected[i]), unsigned(actual[i]));
				}
				cipher.RC4Crypt(nullptr, nullptr, length);
				reference.DiscardBytes(length);
			}
			// Observe the last skip too; otherwise a final discard failure
			// would never affect any compared output.
			std::array<uint8, 16> actual{}, expected{};
			cipher.RC4Crypt(actual.data(), actual.data(), actual.size());
			reference.GenerateBlock(expected.data(), expected.size());
			for (size_t i = 0; i < actual.size(); ++i) {
				ASSERT_EQUALS(unsigned(expected[i]), unsigned(actual[i]));
			}
		}
	}
}

TEST(RC4, ChunkingInPlaceAndDiscardPreserveStream)
{
	const MD5Sum key(wxT("RC4 streaming regression"));
	for (bool skipDiscard : { false, true }) {
		CRC4EncryptableBuffer whole, chunked;
		whole.SetKey(key, skipDiscard);
		chunked.SetKey(key, skipDiscard);
		std::vector<uint8> input(8192);
		for (size_t i = 0; i < input.size(); ++i) {
			input[i] = static_cast<uint8>(i * 31);
		}
		auto expected = input;
		auto actual = input;
		whole.RC4Crypt(input.data(), expected.data(), input.size());
		uint32 offset = 0;
		for (uint32 length : { 1u, 4u, 255u, 256u, 257u, 1300u }) {
			chunked.RC4Crypt(actual.data() + offset, actual.data() + offset, length);
			offset += length;
		}
		for (uint32 i = 0; i < offset; ++i) {
			ASSERT_EQUALS(unsigned(expected[i]), unsigned(actual[i]));
		}
		chunked.RC4Crypt(nullptr, nullptr, 1024);
		offset += 1024;
		chunked.RC4Crypt(actual.data() + offset, actual.data() + offset, actual.size() - offset);
		for (size_t i = offset; i < actual.size(); ++i) {
			ASSERT_EQUALS(unsigned(expected[i]), unsigned(actual[i]));
		}
	}
}

TEST(RC4, ResetDataKeepsKeyAndFullResetAllowsRekey)
{
	const MD5Sum key(wxT("RC4 reset regression"));
	const MD5Sum newKey(wxT("RC4 different key after full reset"));
	for (bool skipDiscard : { false, true }) {
		CRC4EncryptableBuffer cipher;
		cipher.SetKey(key, skipDiscard);
		CryptoPP::Weak::ARC4 reference(key.GetRawHash(), 16);
		if (!skipDiscard) {
			reference.DiscardBytes(1024);
		}
		const std::array<uint8, 17> plain{};
		std::array<uint8, 17> expected{};
		for (unsigned i = 0; i < 2; ++i) {
			cipher.Append(plain.data(), plain.size());
			cipher.Encrypt();
			reference.ProcessData(expected.data(), plain.data(), plain.size());
			ASSERT_EQUALS(plain.size(), cipher.GetLength());
			for (size_t j = 0; j < expected.size(); ++j) {
				ASSERT_EQUALS(unsigned(expected[j]), unsigned(cipher.GetRawBuffer()[j]));
			}
			cipher.ResetData();
			ASSERT_EQUALS(0u, cipher.GetLength());
		}
		// Leave encrypted data in the buffer so FullReset must clear both
		// the data/encryption flag and the old key/permutation.
		cipher.Append(plain.data(), plain.size());
		cipher.Encrypt();
		cipher.FullReset();
		ASSERT_EQUALS(0u, cipher.GetLength());
		cipher.SetKey(newKey, skipDiscard);
		reference.SetKey(newKey.GetRawHash(), 16);
		if (!skipDiscard) {
			reference.DiscardBytes(1024);
		}
		cipher.Append(plain.data(), plain.size());
		cipher.Encrypt();
		reference.ProcessData(expected.data(), plain.data(), plain.size());
		ASSERT_EQUALS(plain.size(), cipher.GetLength());
		for (size_t j = 0; j < expected.size(); ++j) {
			ASSERT_EQUALS(unsigned(expected[j]), unsigned(cipher.GetRawBuffer()[j]));
		}
	}
}

TEST(RC4, MixedDatagramCallsMatchIndependentCipher)
{
	// Vary call boundaries, data and discard lengths. Tiny calls and index
	// wraparound exercise the same shared permutation as bulk encryption.
	for (unsigned seed = 1; seed <= 32; ++seed) {
		const uint8 material = static_cast<uint8>(seed);
		const MD5Sum key(&material, 1);
		CRC4EncryptableBuffer cipher;
		const bool skipDiscard = seed % 2 != 0;
		cipher.SetKey(key, skipDiscard);
		CryptoPP::Weak::ARC4 reference(key.GetRawHash(), 16);
		if (!skipDiscard) {
			reference.DiscardBytes(1024);
		}
		uint32 random = seed;
		for (unsigned call = 0; call < 128; ++call) {
			random = random * 1664525u + 1013904223u;
			const uint32 length = 1 + ((random >> 16) % 521);
			if (call % 4 == 0) {
				std::vector<uint8> untouched(length, 0xa5);
				// Null input means discard even with a non-null output.
				cipher.RC4Crypt(nullptr, untouched.data(), length);
				reference.DiscardBytes(length);
				for (uint8 byte : untouched) {
					ASSERT_EQUALS(0xa5u, unsigned(byte));
				}
			} else {
				std::vector<uint8> input(length), actual(length), expected(length);
				for (uint32 i = 0; i < length; ++i) {
					random = random * 1664525u + 1013904223u;
					input[i] = static_cast<uint8>(random >> 24);
				}
				reference.ProcessData(expected.data(), input.data(), length);
				if (call % 2) {
					actual = input;
					cipher.RC4Crypt(actual.data(), actual.data(), length);
				} else {
					cipher.RC4Crypt(input.data(), actual.data(), length);
				}
				for (uint32 i = 0; i < length; ++i) {
					ASSERT_EQUALS(unsigned(expected[i]), unsigned(actual[i]));
				}
			}
		}
	}
}
