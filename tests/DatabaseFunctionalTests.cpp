#include "gtest/gtest.h"
#include "Database.h"
#include <cstdio>
#include <string>

// Functional tests: UTXO lifecycle, block chains, asset operations

class DatabaseFuncTest : public ::testing::Test {
protected:
    std::string dbFile;
    Database* db = nullptr;

    void SetUp() override {
        static int counter = 0;
        dbFile = "_testFunc_" + std::to_string(counter++) + ".db";
        std::remove(dbFile.c_str());
        db = new Database(dbFile);
    }

    void TearDown() override {
        delete db;
        db = nullptr;
        std::remove(dbFile.c_str());
    }

    // Helper: insert a chain of blocks
    void insertBlockChain(unsigned int from, unsigned int to) {
        std::string hash = "aabbccddaabbccddaabbccddaabbccddaabbccddaabbccddaabbccddaabbccdd";
        for (unsigned int h = from; h <= to; h++) {
            db->insertBlock(h, hash, 1389393000 + h, h % 5, 0.001 * h);
        }
    }
};

// --- Block Chain Tests ---

TEST_F(DatabaseFuncTest, BlockChainSequence) {
    insertBlockChain(2, 100);
    EXPECT_EQ(db->getBlockHeight(), 100u);
}

TEST_F(DatabaseFuncTest, BlockChainRewind) {
    insertBlockChain(2, 50);
    EXPECT_EQ(db->getBlockHeight(), 50u);

    db->clearBlocksAboveHeight(25);
    EXPECT_EQ(db->getBlockHeight(), 25u);
}

TEST_F(DatabaseFuncTest, BlockChainRewindAndRebuild) {
    insertBlockChain(2, 50);
    db->clearBlocksAboveHeight(25);
    insertBlockChain(26, 75);
    EXPECT_EQ(db->getBlockHeight(), 75u);
}

TEST_F(DatabaseFuncTest, BlockInsertOrReplaceIdempotent) {
    std::string hash = "aabbccddaabbccddaabbccddaabbccddaabbccddaabbccddaabbccddaabbccdd";
    db->insertBlock(2, hash, 100, 1, 0.5);
    db->insertBlock(2, hash, 100, 1, 0.5); // duplicate
    db->insertBlock(2, hash, 200, 2, 1.0); // update
    EXPECT_EQ(db->getBlockHeight(), 2u);
}

// --- Transaction Batching Tests ---

TEST_F(DatabaseFuncTest, TransactionBatchInsert) {
    db->startTransaction();
    insertBlockChain(2, 101);
    db->endTransaction();
    EXPECT_EQ(db->getBlockHeight(), 101u);
}

TEST_F(DatabaseFuncTest, NestedTransactionDepth3) {
    db->startTransaction();
    db->startTransaction();
    db->startTransaction();
    insertBlockChain(2, 10);
    db->endTransaction();
    db->endTransaction();
    db->endTransaction();
    EXPECT_EQ(db->getBlockHeight(), 10u);
}

TEST_F(DatabaseFuncTest, TransactionRollbackSafety) {
    // Simulate: start transaction, insert, end transaction
    // Then verify data persists
    db->startTransaction();
    std::string hash = "aabbccddaabbccddaabbccddaabbccddaabbccddaabbccddaabbccddaabbccdd";
    db->insertBlock(2, hash, 100, 1, 0.5);
    db->endTransaction();

    // Reopen DB and verify
    delete db;
    db = new Database(dbFile);
    EXPECT_EQ(db->getBlockHeight(), 2u);
}

// --- Flag Persistence Tests ---

TEST_F(DatabaseFuncTest, FlagPersistsAcrossReopen) {
    db->setBeenPrunedNonAssetUTXOHistory(true);
    delete db;
    db = new Database(dbFile);
    EXPECT_TRUE(db->getBeenPrunedNonAssetUTXOHistory());
}

// --- Write Verification Modes ---

TEST_F(DatabaseFuncTest, SyncModeAfterDisableVerification) {
    db->disableWriteVerification();
    // Should still function correctly
    insertBlockChain(2, 50);
    EXPECT_EQ(db->getBlockHeight(), 50u);

    // Reopen and verify persistence (MEMORY journal may lose data on crash
    // but clean close should persist)
    delete db;
    db = new Database(dbFile);
    EXPECT_EQ(db->getBlockHeight(), 50u);
}

// --- Asset Count Tests ---

TEST_F(DatabaseFuncTest, AssetCountStartsAtOne) {
    // DigiByte native asset is index 1
    EXPECT_EQ(db->getAssetCountOnChain(), 1u);
}

// --- ClearBlocksAboveHeight Edge Cases ---

TEST_F(DatabaseFuncTest, ClearAtHeight1DoesNotDeleteGenesis) {
    db->clearBlocksAboveHeight(1);
    EXPECT_EQ(db->getBlockHeight(), 1u);
}

TEST_F(DatabaseFuncTest, ClearAboveHigherThanMax) {
    insertBlockChain(2, 10);
    db->clearBlocksAboveHeight(999);
    EXPECT_EQ(db->getBlockHeight(), 10u); // nothing deleted
}

// --- Regression: SQLite return code checks ---

TEST_F(DatabaseFuncTest, RegressionInsertReturnCode) {
    // This tests that INSERT statements check SQLITE_DONE not SQLITE_OK
    // The addWatchAddress bug was checking != SQLITE_OK which always threw
    std::string hash = "aabbccddaabbccddaabbccddaabbccddaabbccddaabbccddaabbccddaabbccdd";
    EXPECT_NO_THROW(db->insertBlock(2, hash, 100, 1, 0.5));
}
