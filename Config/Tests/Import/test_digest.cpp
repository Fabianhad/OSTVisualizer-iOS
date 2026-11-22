// SHA-1 / SHA-256 of Core/Import/Digest.hpp. The OSP image staging puts the first 16 hex digits of sha1(member name) into a folder name and
// the sha256 of a file into a file name (IMP-12), so both must be hashlib's. Every expected digest below was computed with the Windows
// interpreter's hashlib (python -B; FIPS 180-4 test vectors for "abc", the empty message, the 448 bit message and one million 'a').
#include "TestHarness.hpp"

#include "Import/Digest.hpp"

#include <string>

using ost_import::digest::Sha1;
using ost_import::digest::Sha256;

TEST(Digest, FipsVectors)
{
    CHECK_STR_EQ(ost_import::digest::sha1_hex(""), "da39a3ee5e6b4b0d3255bfef95601890afd80709");
    CHECK_STR_EQ(ost_import::digest::sha1_hex("abc"), "a9993e364706816aba3e25717850c26c9cd0d89d");
    CHECK_STR_EQ(ost_import::digest::sha1_hex("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"),
                 "84983e441c3bd26ebaae4aa1f95129e5e54670f1");
    CHECK_STR_EQ(ost_import::digest::sha256_hex(""), "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    CHECK_STR_EQ(ost_import::digest::sha256_hex("abc"), "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    CHECK_STR_EQ(ost_import::digest::sha256_hex("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"),
                 "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
}

// hashlib.sha256(b"new image") and (b"new drawing"): the digests of the Windows image staging tests.
TEST(Digest, TheStagingContentDigests)
{
    CHECK_STR_EQ(ost_import::digest::sha256_hex("new image"), "41c04b3f2d0c69a66494faa96f05b622709379f3a5251078e0cd39fa9c44d67e");
    CHECK_STR_EQ(ost_import::digest::sha256_hex("new drawing"), "f9ee0dc8058e14f8a942b73cf2155cced4120b1b678c11f1a4cf459c3cea834c");
    CHECK_STR_EQ(ost_import::digest::sha256_hex("alpha"), "8ed3f6ad685b959ead7022518e1af76cd816f8e8ec7ccdda1ed4018e8f2223f8");
    CHECK_STR_EQ(ost_import::digest::sha256_hex("bravo"), "f144a6907dc4284d1f9fe6a7d9b9ff53c02c1d07ba68f24d413d7ff7f757a782");
}

// The padding changes shape at 55/56 and 63/64/65 bytes: messages of those lengths (byte i = (i*7+3) mod 256).
TEST(Digest, PaddingBoundaries)
{
    struct Case
    {
        size_t length;
        const char *sha1;
        const char *sha256;
    };
    const Case cases[] = {
        {55, "ddf57317ef34bfee3b6df83d359098930eb278bc", "e7313d333c272e639f790978283f9eb392e843d0f29b7016828bb1daa4aac70b"},
        {56, "a0d492bb0fc889d0eca3bc137066ab6f4f74f369", "4324d65f3c103567f5589c710bc08f8523f929a9272e3af36fc968e52abc6c27"},
        {57, "11a02dcf95859677a62e75024067c22b165d890f", "35df609437dcfea3279283ab79fd554e2bf78f8f7ae2de532d8ee300b09e8f73"},
        {63, "c55856749bef509bdfe6bfebfc7bf4e793e82132", "81c80242132f230c3bd41b3e63bbcff16107339549214a99614ff26664625055"},
        {64, "bede92be29c3874e1b54ddc77988d606fc857a8e", "39e3d7b6b5d075d37d053ad89b24b41bef4f3c29760c84447cab3f3be1882241"},
        {65, "b05a80522b053d6dc7e0a517d0e70212c7dad11f", "aacca6ff74fdbb296d165a45cecfa04e5127bc008770fbbdd48006f2d2fae95e"},
        {119, "504e27376a6e0f0dba8295b85cb25dc4dfa17d23", "9ce7368e4daf32341631b492e80359dc9f594b48453cd0dd5bf0b19279cc177e"},
        {120, "82134b02fb3f702491be9bed581eeab59334acb2", "7836b787757e95e58b3ca5aec90b1b004e8deba1e50e9675af9cabf1a13a04b5"},
        {128, "a09133e6730ffe899efb70204cb5646cd5dc24ee", "d2742f1f4ac6bb7ca2b239ee18402ba8b3f9f8e652d2a72973c2b9ba11c08cf6"},
    };
    for (const Case &c : cases)
    {
        SCOPED_CONTEXT("length " + std::to_string(c.length));
        std::string data;
        for (size_t i = 0; i < c.length; ++i)
            data += static_cast<char>((i * 7 + 3) % 256);
        CHECK_STR_EQ(ost_import::digest::sha1_hex(data), c.sha1);
        CHECK_STR_EQ(ost_import::digest::sha256_hex(data), c.sha256);
        // fed in odd pieces the result is the same
        Sha1 one;
        Sha256 two;
        for (size_t pos = 0; pos < data.size(); pos += 7)
        {
            const std::string piece = data.substr(pos, 7);
            one.update(piece.data(), piece.size());
            two.update(piece.data(), piece.size());
        }
        CHECK_STR_EQ(one.hex(), c.sha1);
        CHECK_STR_EQ(two.hex(), c.sha256);
    }
}

TEST(Digest, OneMillionAsAndAReusableObject)
{
    Sha1 one;
    Sha256 two;
    const std::string block(1000, 'a');
    for (int i = 0; i < 1000; ++i)
    {
        one.update(block.data(), block.size());
        two.update(block.data(), block.size());
    }
    CHECK_STR_EQ(one.hex(), "34aa973cd4c4daa4f61eeb2bdbad27316534016f");
    CHECK_STR_EQ(two.hex(), "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
    // hex() does not consume the state
    CHECK_STR_EQ(one.hex(), "34aa973cd4c4daa4f61eeb2bdbad27316534016f");
    one.update("b", 1);
    CHECK_STR_EQ(one.hex(), ost_import::digest::sha1_hex(std::string(1000000, 'a') + "b"));
}
