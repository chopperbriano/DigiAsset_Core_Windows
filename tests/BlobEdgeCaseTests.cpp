#include "gtest/gtest.h"
#include "Blob.h"
#include <vector>
#include <string>

// Edge cases for Blob hex conversion

TEST(BlobEdge, SingleByte) {
    Blob b("ff");
    EXPECT_EQ(b.length(), 1u);
    EXPECT_EQ(b.toHex(), "ff");
}

TEST(BlobEdge, MixedCase) {
    Blob b("aAbBcCdD");
    EXPECT_EQ(b.toHex(), "aabbccdd");
}

TEST(BlobEdge, LargeBlob) {
    // 256 bytes = 512 hex chars
    std::string hex;
    for (int i = 0; i < 256; i++) {
        char buf[3];
        snprintf(buf, sizeof(buf), "%02x", i);
        hex += buf;
    }
    Blob b(hex);
    EXPECT_EQ(b.length(), 256u);
    EXPECT_EQ(b.toHex(), hex);
}

TEST(BlobEdge, VectorRoundTrip) {
    std::vector<uint8_t> original = {0, 1, 127, 128, 254, 255};
    Blob b(original);
    auto result = b.vector();
    EXPECT_EQ(result, original);
}

TEST(BlobEdge, RawDataConstructor) {
    uint8_t data[] = {0xDE, 0xAD, 0xBE, 0xEF};
    Blob b(data, 4);
    EXPECT_EQ(b.toHex(), "deadbeef");
    EXPECT_EQ(b.length(), 4u);
}

TEST(BlobEdge, SelfAssignment) {
    Blob b("abcd");
    b = b;
    EXPECT_EQ(b.toHex(), "abcd");
}

TEST(BlobEdge, MultipleAssignments) {
    Blob a("1111");
    Blob b("2222");
    Blob c("3333");
    a = b;
    b = c;
    EXPECT_EQ(a.toHex(), "2222");
    EXPECT_EQ(b.toHex(), "3333");
    EXPECT_EQ(c.toHex(), "3333");
}

TEST(BlobEdge, InequalityDifferentLength) {
    Blob a("aabb");
    Blob b("aabbcc");
    EXPECT_TRUE(a != b);
}

TEST(BlobEdge, TxidSizeBlob) {
    // Standard 32-byte txid
    std::string txid = "e77c20f2f70b46f54e142a1ab296bd8176f389880062b3eddbac038da73ec87d";
    Blob b(txid);
    EXPECT_EQ(b.length(), 32u);
    EXPECT_EQ(b.toHex(), txid);
}
