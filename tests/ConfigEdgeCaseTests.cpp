#include "gtest/gtest.h"
#include "Config.h"
#include <fstream>
#include <cstdio>

class ConfigEdgeTest : public ::testing::Test {
protected:
    std::string tempFile = "_test_config_edge.cfg";

    void writeConfig(const std::string& content) {
        std::ofstream f(tempFile);
        f << content;
        f.close();
    }

    void TearDown() override {
        std::remove(tempFile.c_str());
    }
};

TEST_F(ConfigEdgeTest, EmptyFile) {
    writeConfig("");
    Config c(tempFile);
    EXPECT_EQ(c.getString("anything", "default"), "default");
}

TEST_F(ConfigEdgeTest, OnlyComments) {
    writeConfig("# comment 1\n# comment 2\n# comment 3\n");
    Config c(tempFile);
    EXPECT_EQ(c.getInteger("x", 42), 42);
}

TEST_F(ConfigEdgeTest, ValueWithEquals) {
    writeConfig("url=http://host:8080/path?a=b&c=d\n");
    Config c(tempFile);
    EXPECT_EQ(c.getString("url"), "http://host:8080/path?a=b&c=d");
}

TEST_F(ConfigEdgeTest, EmptyValue) {
    // Config parser treats "key=" (empty value) as no value — key is not stored
    writeConfig("emptykey=\n");
    Config c(tempFile);
    EXPECT_EQ(c.getString("emptykey", "default"), "default");
}

TEST_F(ConfigEdgeTest, NumericStringAsString) {
    writeConfig("port=14022\n");
    Config c(tempFile);
    // Should work as both string and integer
    EXPECT_EQ(c.getString("port"), "14022");
    EXPECT_EQ(c.getInteger("port"), 14022);
}

TEST_F(ConfigEdgeTest, NegativeInteger) {
    writeConfig("pruneage=-1\n");
    Config c(tempFile);
    EXPECT_EQ(c.getInteger("pruneage"), -1);
}

TEST_F(ConfigEdgeTest, ZeroInteger) {
    writeConfig("val=0\n");
    Config c(tempFile);
    EXPECT_EQ(c.getInteger("val"), 0);
    EXPECT_FALSE(c.getBool("val"));
}

TEST_F(ConfigEdgeTest, LargeInteger) {
    writeConfig("big=2147483647\n");
    Config c(tempFile);
    EXPECT_EQ(c.getInteger("big"), 2147483647);
}

TEST_F(ConfigEdgeTest, MultipleKeysPreservesAll) {
    writeConfig("a=1\nb=2\nc=3\nd=4\ne=5\n");
    Config c(tempFile);
    EXPECT_EQ(c.getInteger("a"), 1);
    EXPECT_EQ(c.getInteger("e"), 5);
}

TEST_F(ConfigEdgeTest, DigiByteCoreStyleConfig) {
    // Realistic config matching what users have
    writeConfig(
        "bootstrapchainstate=0\n"
        "verifydatabasewrite=0\n"
        "pruneage=5760\n"
        "ipfspath=http://localhost:5001/api/v0/\n"
        "psp0payout=dgb1qh9n2zzuhdd37gyrktjam5uju8gy3f5ems4yna3\n"
        "rpcallow*=1\n"
        "rpcbind=127.0.0.1\n"
        "rpcpassword=fred1\n"
        "rpcport=14022\n"
        "rpcuser=fred\n"
    );
    Config c(tempFile);
    EXPECT_FALSE(c.getBool("bootstrapchainstate"));
    EXPECT_FALSE(c.getBool("verifydatabasewrite"));
    EXPECT_EQ(c.getInteger("pruneage"), 5760);
    EXPECT_EQ(c.getString("rpcbind"), "127.0.0.1");
    EXPECT_EQ(c.getInteger("rpcport"), 14022);
    EXPECT_EQ(c.getString("rpcuser"), "fred");
    EXPECT_EQ(c.getString("psp0payout"), "dgb1qh9n2zzuhdd37gyrktjam5uju8gy3f5ems4yna3");

    auto allowed = c.getBoolMap("rpcallow");
    EXPECT_TRUE(allowed["*"]);
}

TEST_F(ConfigEdgeTest, WritePreservesTypes) {
    Config c;
    c.setString("s", "hello");
    c.setInteger("i", -42);
    c.setBool("b", true);
    c.write(tempFile);

    Config c2(tempFile);
    EXPECT_EQ(c2.getString("s"), "hello");
    EXPECT_EQ(c2.getInteger("i"), -42);
    EXPECT_TRUE(c2.getBool("b"));
}
