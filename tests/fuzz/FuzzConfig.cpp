// Fuzz test for Config parser
// Tests that arbitrary input doesn't crash the config parser
// Build: cl /EHsc /I../../src FuzzConfig.cpp ../../src/Config.cpp /Fe:fuzz_config.exe
// Run:   generate random input and pipe, or run standalone for self-test

#include "Config.h"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <fstream>
#include <cassert>

static const char* FUZZ_FILE = "_fuzz_config_temp.cfg";

static int testOne(const uint8_t* data, size_t size) {
    // Write fuzzed data to temp config file
    {
        std::ofstream f(FUZZ_FILE, std::ios::binary);
        f.write((const char*)data, size);
    }

    try {
        Config c(FUZZ_FILE);

        // Try various operations — none should crash
        try { c.getString("key"); } catch (...) {}
        try { c.getString("key", "default"); } catch (...) {}
        try { c.getInteger("key"); } catch (...) {}
        try { c.getInteger("key", 42); } catch (...) {}
        try { c.getBool("key"); } catch (...) {}
        try { c.getBool("key", false); } catch (...) {}
        try { c.getBoolMap("rpc"); } catch (...) {}
        try { c.getStringMap("rpc"); } catch (...) {}
        try { c.getIntegerMap("rpc"); } catch (...) {}

        // Try to write back out
        try { c.write(FUZZ_FILE); } catch (...) {}
    } catch (...) {
        // Config constructor may throw — that's fine
    }

    std::remove(FUZZ_FILE);
    return 0;
}

int main() {
    uint8_t buf[100000];
    size_t len = fread(buf, 1, sizeof(buf), stdin);
    if (len == 0) {
        // Self-test
        const char* test1 = "key=value\nnum=42\nbool=true\n";
        assert(testOne((const uint8_t*)test1, strlen(test1)) == 0);

        const char* test2 = ""; // empty
        assert(testOne((const uint8_t*)test2, 0) == 0);

        const char* test3 = "###\n\n\n===\nk=\n"; // weird
        assert(testOne((const uint8_t*)test3, strlen(test3)) == 0);

        // Binary garbage
        uint8_t test4[256];
        for (int i = 0; i < 256; i++) test4[i] = (uint8_t)i;
        assert(testOne(test4, 256) == 0);

        printf("PASS: Config fuzz self-test\n");
        return 0;
    }
    return testOne(buf, len);
}
