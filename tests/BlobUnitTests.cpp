#include "gtest/gtest.h"
#include "Blob.h"
#include <vector>

TEST(Blob, HexRoundTrip) {
    Blob b("deadbeef");
    EXPECT_EQ(b.toHex(), "deadbeef");
    EXPECT_EQ(b.length(), 4u);
}

TEST(Blob, AllZeros) {
    Blob b("0000");
    EXPECT_EQ(b.toHex(), "0000");
    EXPECT_EQ(b.length(), 2u);
}

TEST(Blob, AllFF) {
    Blob b("ffff");
    EXPECT_EQ(b.toHex(), "ffff");
    EXPECT_EQ(b.length(), 2u);
}

TEST(Blob, UpperCaseHex) {
    Blob b("DEADBEEF");
    EXPECT_EQ(b.toHex(), "deadbeef");
}

TEST(Blob, LongHash) {
    // 64 hex chars = 32 bytes (typical txid)
    std::string hex = "4da631f2ac1bed857bd968c67c913978274d8aabed64ab2bcebc1665d7f4d3a0";
    Blob b(hex);
    EXPECT_EQ(b.toHex(), hex);
    EXPECT_EQ(b.length(), 32u);
}

TEST(Blob, VectorConstructor) {
    std::vector<uint8_t> data = {0xDE, 0xAD, 0xBE, 0xEF};
    Blob b(data);
    EXPECT_EQ(b.toHex(), "deadbeef");
    EXPECT_EQ(b.vector(), data);
}

TEST(Blob, CopyConstructor) {
    Blob original("abcd1234");
    Blob copy(original);
    EXPECT_EQ(copy.toHex(), "abcd1234");
    EXPECT_EQ(original.toHex(), "abcd1234");
}

TEST(Blob, AssignmentOperator) {
    Blob a("1111");
    Blob b("2222");
    a = b;
    EXPECT_EQ(a.toHex(), "2222");
    EXPECT_EQ(b.toHex(), "2222");
}

TEST(Blob, Equality) {
    Blob a("deadbeef");
    Blob b("deadbeef");
    Blob c("aaaabbbb");
    EXPECT_TRUE(a == b);
    EXPECT_TRUE(a != c);
}
