#include "gtest/gtest.h"
#include "Config.h"
#include <fstream>
#include <cstdio>

class ConfigTest : public ::testing::Test {
protected:
    std::string tempFile = "_test_config_unit.cfg";

    void writeConfig(const std::string& content) {
        std::ofstream f(tempFile);
        f << content;
        f.close();
    }

    void TearDown() override {
        std::remove(tempFile.c_str());
    }
};

TEST_F(ConfigTest, ParseValidConfig) {
    writeConfig("rpcuser=fred\nrpcport=14022\nenabled=1\n");
    Config c(tempFile);
    EXPECT_EQ(c.getString("rpcuser"), "fred");
    EXPECT_EQ(c.getInteger("rpcport"), 14022);
    EXPECT_EQ(c.getBool("enabled"), true);
}

TEST_F(ConfigTest, MissingKeyThrows) {
    writeConfig("foo=bar\n");
    Config c(tempFile);
    EXPECT_THROW(c.getString("missing"), Config::exceptionCorruptConfigFile);
}

TEST_F(ConfigTest, DefaultValues) {
    writeConfig("");
    Config c(tempFile);
    EXPECT_EQ(c.getString("missing", "default"), "default");
    EXPECT_EQ(c.getInteger("missing", 42), 42);
    EXPECT_EQ(c.getBool("missing", true), true);
}

TEST_F(ConfigTest, BoolParsing) {
    writeConfig("a=true\nb=false\nc=1\nd=0\n");
    Config c(tempFile);
    EXPECT_TRUE(c.getBool("a"));
    EXPECT_FALSE(c.getBool("b"));
    EXPECT_TRUE(c.getBool("c"));
    EXPECT_FALSE(c.getBool("d"));
}

TEST_F(ConfigTest, CommentsAndBlankLines) {
    writeConfig("# comment\n\nfoo=bar\n# another comment\nbaz=42\n");
    Config c(tempFile);
    EXPECT_EQ(c.getString("foo"), "bar");
    EXPECT_EQ(c.getInteger("baz"), 42);
}

TEST_F(ConfigTest, WriteAndReRead) {
    Config c;
    c.setString("user", "alice");
    c.setInteger("port", 8080);
    c.setBool("enabled", true);
    c.write(tempFile);

    Config c2(tempFile);
    EXPECT_EQ(c2.getString("user"), "alice");
    EXPECT_EQ(c2.getInteger("port"), 8080);
    EXPECT_TRUE(c2.getBool("enabled"));
}

TEST_F(ConfigTest, MapPrefixFiltering) {
    writeConfig("rpcallowfoo=1\nrpcallowbar=0\nunrelated=5\n");
    Config c(tempFile);
    auto m = c.getBoolMap("rpcallow");
    EXPECT_EQ(m.size(), 2u);
    EXPECT_TRUE(m["foo"]);
    EXPECT_FALSE(m["bar"]);
}
