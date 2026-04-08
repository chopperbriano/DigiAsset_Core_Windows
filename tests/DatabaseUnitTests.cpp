#include "gtest/gtest.h"
#include "Database.h"
#include <cstdio>

class DatabaseTest : public ::testing::Test {
protected:
    std::string dbFile;
    Database* db = nullptr;

    void SetUp() override {
        static int counter = 0;
        dbFile = "_testUnit_" + std::to_string(counter++) + ".db";
        std::remove(dbFile.c_str());
        db = new Database(dbFile);
    }

    void TearDown() override {
        delete db;
        db = nullptr;
        std::remove(dbFile.c_str());
    }
};

TEST_F(DatabaseTest, CreateAndOpen) {
    // Fresh DB should have height 1 (genesis block from buildTables)
    EXPECT_EQ(db->getBlockHeight(), 1u);
}

TEST_F(DatabaseTest, InsertBlockAndRetrieve) {
    std::string hash = "aabbccddaabbccddaabbccddaabbccddaabbccddaabbccddaabbccddaabbccdd";
    db->insertBlock(2, hash, 1389393000, 1, 0.001);
    EXPECT_EQ(db->getBlockHeight(), 2u);
}

TEST_F(DatabaseTest, InsertBlockOrReplace) {
    // INSERT OR REPLACE should not throw on duplicate
    std::string hash = "aabbccddaabbccddaabbccddaabbccddaabbccddaabbccddaabbccddaabbccdd";
    db->insertBlock(2, hash, 1389393000, 1, 0.001);
    EXPECT_NO_THROW(db->insertBlock(2, hash, 1389393000, 1, 0.001));
    EXPECT_EQ(db->getBlockHeight(), 2u);
}

TEST_F(DatabaseTest, FlagRoundTrip) {
    db->setBeenPrunedNonAssetUTXOHistory(true);
    EXPECT_TRUE(db->getBeenPrunedNonAssetUTXOHistory());
    db->setBeenPrunedNonAssetUTXOHistory(false);
    EXPECT_FALSE(db->getBeenPrunedNonAssetUTXOHistory());
}

TEST_F(DatabaseTest, ClearBlocksAboveHeight) {
    std::string hash = "aabbccddaabbccddaabbccddaabbccddaabbccddaabbccddaabbccddaabbccdd";
    db->insertBlock(2, hash, 100, 1, 0.001);
    db->insertBlock(3, hash, 200, 1, 0.001);
    db->insertBlock(4, hash, 300, 1, 0.001);
    EXPECT_EQ(db->getBlockHeight(), 4u);

    db->clearBlocksAboveHeight(2);
    EXPECT_EQ(db->getBlockHeight(), 2u);
}

TEST_F(DatabaseTest, TransactionNesting) {
    // Nested transactions should work via depth counter
    db->startTransaction();
    db->startTransaction(); // nested
    db->endTransaction();   // decrements counter, doesn't commit
    db->endTransaction();   // commits
    // Should not crash or leave DB in bad state
    EXPECT_EQ(db->getBlockHeight(), 1u);
}

TEST_F(DatabaseTest, DisableWriteVerification) {
    // Should not throw
    EXPECT_NO_THROW(db->disableWriteVerification());
    // DB should still work after changing pragmas
    std::string hash = "aabbccddaabbccddaabbccddaabbccddaabbccddaabbccddaabbccddaabbccdd";
    db->insertBlock(2, hash, 100, 1, 0.001);
    EXPECT_EQ(db->getBlockHeight(), 2u);
}

TEST_F(DatabaseTest, AssetCountOnChain) {
    // Fresh DB has 1 asset (DigiByte native)
    EXPECT_EQ(db->getAssetCountOnChain(), 1u);
}
