// Fuzz test for Blob hex conversion
// Build: cl /EHsc /I../../src FuzzBlob.cpp ../../src/Blob.cpp /Fe:fuzz_blob.exe
// Run:   generate random input and pipe: python -c "import os; os.write(1,os.urandom(1000))" | fuzz_blob.exe

#include "Blob.h"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <cassert>

// Test a single fuzz input
static int testOne(const uint8_t* data, size_t size) {
    // Test 1: Raw bytes constructor + roundtrip
    if (size > 0 && size < 100000) {
        try {
            Blob b(data, (int)size);
            std::string hex = b.toHex();
            assert(hex.length() == size * 2);
            Blob b2(hex);
            assert(b == b2);
            assert(b.length() == size);
            auto vec = b.vector();
            assert(vec.size() == size);
            for (size_t i = 0; i < size; i++) {
                assert(vec[i] == data[i]);
            }
        } catch (...) {
            // Should not throw for valid raw bytes
            return 1;
        }
    }

    // Test 2: Hex string constructor (only valid hex chars, even length)
    if (size >= 2 && size % 2 == 0 && size < 10000) {
        bool validHex = true;
        for (size_t i = 0; i < size; i++) {
            char c = (char)data[i];
            if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {
                validHex = false;
                break;
            }
        }
        if (validHex) {
            try {
                std::string hex((const char*)data, size);
                Blob b(hex);
                assert(b.length() == size / 2);
                // Roundtrip
                std::string hex2 = b.toHex();
                Blob b2(hex2);
                assert(b == b2);
            } catch (...) {
                return 1; // Valid hex should not throw
            }
        }
    }

    // Test 3: Copy and assignment
    if (size > 0 && size < 1000) {
        Blob original(data, (int)size);
        Blob copy(original);
        assert(copy == original);
        Blob assigned("ff");
        assigned = original;
        assert(assigned == original);
    }

    return 0;
}

#ifdef __AFL_FUZZ_TESTCASE_LEN
// AFL++ mode
__AFL_FUZZ_INIT();
int main() {
    __AFL_INIT();
    unsigned char *buf = __AFL_FUZZ_TESTCASE_BUF;
    while (__AFL_LOOP(10000)) {
        int len = __AFL_FUZZ_TESTCASE_LEN;
        testOne(buf, len);
    }
    return 0;
}
#else
// Standalone mode: read from stdin
int main() {
    uint8_t buf[100000];
    size_t len = fread(buf, 1, sizeof(buf), stdin);
    if (len == 0) {
        // Self-test with known inputs
        uint8_t test1[] = {0xDE, 0xAD, 0xBE, 0xEF};
        assert(testOne(test1, 4) == 0);

        uint8_t test2[] = "deadbeef";
        assert(testOne(test2, 8) == 0);

        uint8_t test3[] = {0, 0, 0, 0, 255, 255, 255, 255};
        assert(testOne(test3, 8) == 0);

        printf("PASS: Blob fuzz self-test\n");
        return 0;
    }
    return testOne(buf, len);
}
#endif
