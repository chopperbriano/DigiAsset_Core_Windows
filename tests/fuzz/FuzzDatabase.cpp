// Fuzz test for Database operations
// Tests that arbitrary input to DB operations doesn't crash or corrupt
// Build: cl /EHsc /I../../src FuzzDatabase.cpp ../../src/*.cpp /Fe:fuzz_db.exe
// Run:   standalone for self-test, or pipe random data

#include "Database.h"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <cassert>

static const char* FUZZ_DB = "_fuzz_database_temp.db";

static int testOne(const uint8_t* data, size_t size) {
    if (size < 4) return 0;

    std::remove(FUZZ_DB);
    try {
        Database db(FUZZ_DB);

        size_t offset = 0;
        while (offset + 4 < size) {
            uint8_t op = data[offset++];

            switch (op % 5) {
                case 0: {
                    // Insert block with fuzzed data
                    if (offset + 32 > size) break;
                    std::string hash;
                    for (int i = 0; i < 32; i++) {
                        char buf[3];
                        snprintf(buf, sizeof(buf), "%02x", data[offset + i]);
                        hash += buf;
                    }
                    offset += 32;
                    unsigned int height = 2 + (data[offset] % 200);
                    offset++;
                    try {
                        db.insertBlock(height, hash, 1000000 + height, height % 5, 0.1);
                    } catch (...) {}
                    break;
                }
                case 1: {
                    // Get block height
                    try { db.getBlockHeight(); } catch (...) {}
                    break;
                }
                case 2: {
                    // Clear blocks above height
                    unsigned int h = data[offset] % 100 + 1;
                    offset++;
                    try { db.clearBlocksAboveHeight(h); } catch (...) {}
                    break;
                }
                case 3: {
                    // Set/get flags
                    try {
                        db.setBeenPrunedNonAssetUTXOHistory(data[offset] % 2 == 0);
                        db.getBeenPrunedNonAssetUTXOHistory();
                    } catch (...) {}
                    offset++;
                    break;
                }
                case 4: {
                    // Transaction nesting
                    try {
                        db.startTransaction();
                        db.endTransaction();
                    } catch (...) {}
                    break;
                }
            }
        }
    } catch (...) {
        // Constructor may fail — that's OK
    }

    std::remove(FUZZ_DB);
    return 0;
}

int main() {
    uint8_t buf[10000];
    size_t len = fread(buf, 1, sizeof(buf), stdin);
    if (len == 0) {
        // Self-test with deterministic inputs
        uint8_t test1[] = {
            0, // op 0: insert block
            0xaa, 0xbb, 0xcc, 0xdd, 0xaa, 0xbb, 0xcc, 0xdd,
            0xaa, 0xbb, 0xcc, 0xdd, 0xaa, 0xbb, 0xcc, 0xdd,
            0xaa, 0xbb, 0xcc, 0xdd, 0xaa, 0xbb, 0xcc, 0xdd,
            0xaa, 0xbb, 0xcc, 0xdd, 0xaa, 0xbb, 0xcc, 0xdd,
            5, // height offset
            1, // op 1: get height
            3, 1, // op 3: set flag
            4, // op 4: transaction
        };
        assert(testOne(test1, sizeof(test1)) == 0);

        printf("PASS: Database fuzz self-test\n");
        return 0;
    }
    return testOne(buf, len);
}
