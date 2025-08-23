// LRU, LRU-K, and Hash-LRU test suite with detailed comments
#include <iostream>
#include <string>
#include <random>
#include <iomanip>
#include <thread>
#include "LruCache.h"

using namespace Cache;

// Hot access test: simulate hot key access scenario
void runHotAccessTest(const std::string& testName, int capacity, int hotKeys, int coldKeys, int totalOps, int putRatio, int k = -1) {
    std::cout << "=== " << testName << " ===\n";
    std::random_device rd;
    std::mt19937 gen(rd());

    // Create LRU or LRU-K cache object
    CachePolicy<int, std::string>* cache = nullptr;
    LruCache<int, std::string> lru(capacity);
    LruKCache<int, std::string> lruk(capacity, hotKeys + coldKeys, k);
    cache = (k == -1) ? static_cast<CachePolicy<int, std::string>*>(&lru)
                      : static_cast<CachePolicy<int, std::string>*>(&lruk);

    int getCount = 0, hitCount = 0;
    for (int i = 0; i < totalOps; ++i) {
        // Generate hot or cold key
        bool isPut = static_cast<int>(gen() % 100) < putRatio;
        int key = (gen() % 100 < 70) ? gen() % hotKeys : hotKeys + gen() % coldKeys;

        if (isPut) cache->put(key, "val_" + std::to_string(key));
        else {
            std::string result;
            getCount++;
            if (cache->get(key, result)) hitCount++;
        }
    }
    std::cout << "GETs: " << getCount << ", Hits: " << hitCount << ", Hit Rate: " << std::fixed << std::setprecision(2) << (100.0 * hitCount / getCount) << "%\n\n";
}

// Hot test: for Hash-LRU, compare its slice access performance
void runHotAccessTest_HashLru(const std::string& testName, int capacity, int hotKeys, int coldKeys, int totalOps, int putRatio, int sliceNum = 0) {
    std::cout << "=== " << testName << " ===\n";
    std::random_device rd;
    std::mt19937 gen(rd());
    HashLruCaches<int, std::string> cache(capacity, sliceNum);

    int getCount = 0, hitCount = 0;
    for (int i = 0; i < totalOps; ++i) {
        bool isPut = static_cast<int>(gen() % 100) < putRatio;
        int key = (gen() % 100 < 70) ? gen() % hotKeys : hotKeys + gen() % coldKeys;
        if (isPut) cache.put(key, "val_" + std::to_string(key));
        else {
            std::string result;
            getCount++;
            if (cache.get(key, result)) hitCount++;
        }
    }
    std::cout << "GETs: " << getCount << ", Hits: " << hitCount << ", Hit Rate: " << std::fixed << std::setprecision(2) << (100.0 * hitCount / getCount) << "%\n\n";
}

// Loop scan test: simulate block data access scenario, detect cache hit
void runLoopScanTest(const std::string& testName, int capacity, int loopRange, int totalOps, int putRatio, int k = -1) {
    std::cout << "=== " << testName << " ===\n";
    std::random_device rd;
    std::mt19937 gen(rd());

    CachePolicy<int, std::string>* cache = nullptr;
    LruCache<int, std::string> lru(capacity);
    LruKCache<int, std::string> lruk(capacity, loopRange * 2, k);
    cache = (k == -1) ? static_cast<CachePolicy<int, std::string>*>(&lru)
                      : static_cast<CachePolicy<int, std::string>*>(&lruk);

    int getCount = 0, hitCount = 0, pos = 0;
    for (int i = 0; i < totalOps; ++i) {
        bool isPut = static_cast<int>(gen() % 100) < putRatio;
        int key = pos;
        pos = (pos + 1) % loopRange;
        if (isPut) cache->put(key, "loop_" + std::to_string(key));
        else {
            std::string result;
            getCount++;
            if (cache->get(key, result)) hitCount++;
        }
    }
    std::cout << "GETs: " << getCount << ", Hits: " << hitCount << ", Hit Rate: " << std::fixed << std::setprecision(2) << (100.0 * hitCount / getCount) << "%\n\n";
}

// Loop scan test (Hash-LRU version)
void runLoopScanTest_HashLru(const std::string& testName, int capacity, int loopRange, int totalOps, int putRatio, int sliceNum = 0) {
    std::cout << "=== " << testName << " ===\n";
    std::random_device rd;
    std::mt19937 gen(rd());
    HashLruCaches<int, std::string> cache(capacity, sliceNum);

    int getCount = 0, hitCount = 0, pos = 0;
    for (int i = 0; i < totalOps; ++i) {
        bool isPut = static_cast<int>(gen() % 100) < putRatio;
        int key = pos;
        pos = (pos + 1) % loopRange;
        if (isPut) cache.put(key, "loop_" + std::to_string(key));
        else {
            std::string result;
            getCount++;
            if (cache.get(key, result)) hitCount++;
        }
    }
    std::cout << "GETs: " << getCount << ", Hits: " << hitCount << ", Hit Rate: " << std::fixed << std::setprecision(2) << (100.0 * hitCount / getCount) << "%\n\n";
}

// Cold data test: key that is replaced after one access, test the worst cache hit scenario
void runColdDataTest(const std::string& testName, int capacity, int totalOps, int keyRange, int k = -1) {
    std::cout << "=== " << testName << " ===\n";
    std::random_device rd;
    std::mt19937 gen(rd());
    CachePolicy<int, std::string>* cache = nullptr;
    LruCache<int, std::string> lru(capacity);
    LruKCache<int, std::string> lruk(capacity, keyRange, k);
    cache = (k == -1) ? static_cast<CachePolicy<int, std::string>*>(&lru)
                      : static_cast<CachePolicy<int, std::string>*>(&lruk);

    int getCount = 0, hitCount = 0;
    for (int i = 0; i < totalOps; ++i) {
        int key = gen() % keyRange + 100000; // Ensure key is unique and large, simulate one-time access
        cache->put(key, "cold_" + std::to_string(key));
        std::string result;
        getCount++;
        if (cache->get(key, result)) hitCount++;
    }
    std::cout << "GETs: " << getCount << ", Hits: " << hitCount << ", Hit Rate: " << std::fixed << std::setprecision(2) << (100.0 * hitCount / getCount) << "%\n\n";
}

// Cold data test (Hash-LRU version)
void runColdDataTest_HashLru(const std::string& testName, int capacity, int totalOps, int keyRange, int sliceNum = 0) {
    std::cout << "=== " << testName << " ===\n";
    std::random_device rd;
    std::mt19937 gen(rd());
    HashLruCaches<int, std::string> cache(capacity, sliceNum);

    int getCount = 0, hitCount = 0;
    for (int i = 0; i < totalOps; ++i) {
        int key = gen() % keyRange + 100000;
        cache.put(key, "cold_" + std::to_string(key));
        std::string result;
        getCount++;
        if (cache.get(key, result)) hitCount++;
    }
    std::cout << "GETs: " << getCount << ", Hits: " << hitCount << ", Hit Rate: " << std::fixed << std::setprecision(2) << (100.0 * hitCount / getCount) << "%\n\n";
}

int main() {
    // Hot access test
    runHotAccessTest("Test 1: Baseline (CAPACITY=20, HOT_KEYS=20)", 20, 20, 2000, 100000, 30);
    runHotAccessTest("Test 2: Increase Capacity (CAPACITY=40)", 40, 20, 2000, 100000, 30);
    runHotAccessTest("Test 3: Reduce Hot Keys (HOT_KEYS=10)", 20, 10, 2000, 100000, 30);
    runHotAccessTest("Test 4: High PUT rate (PUT=60%)", 20, 20, 2000, 100000, 60);
    runHotAccessTest("Test 5: LRU-K vs LRU (k=2)", 20, 20, 2000, 100000, 30, 2);
    runHotAccessTest("Test 6: LRU-K (k=3)", 20, 20, 2000, 100000, 30, 3);

    // Loop scan test
    runLoopScanTest("Test 7: Loop Scan LRU", 20, 500, 100000, 20);
    runLoopScanTest("Test 8: Loop Scan LRU-K (k=2)", 20, 500, 100000, 20, 2);

    // Cold data test
    runColdDataTest("Test 9: All Cold Data LRU", 20, 50000, 10000);
    runColdDataTest("Test 10: All Cold Data LRU-K (k=2)", 20, 50000, 10000, 2);

    // Hash-LRU comparison test
    runHotAccessTest_HashLru("Test 11: Hash LRU (default slice)", 20, 20, 2000, 100000, 30);
    runHotAccessTest_HashLru("Test 12: Hash LRU (4 slices)", 20, 20, 2000, 100000, 30, 4);
    runLoopScanTest_HashLru("Test 13: Loop Scan Hash LRU", 20, 500, 100000, 20);
    runColdDataTest_HashLru("Test 14: All Cold Data Hash LRU", 20, 50000, 10000);

    return 0;
}
