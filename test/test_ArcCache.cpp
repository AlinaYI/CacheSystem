#include <iostream>
#include <string>
#include <random>
#include <iomanip>
#include "ArcCache.h"

using namespace Cache;

// Basic test: hot/cold data + PUT/GET mixed
void runArcTest(const std::string& testName,
                int capacity, int hotKeys, int coldKeys,
                int totalOps, int putRatio) {
    std::cout << "=== " << testName << " ===\n";
    std::random_device rd;
    std::mt19937 gen(rd());
    ArcCache<int, std::string> cache(static_cast<size_t>(capacity));

    int getCount = 0, hitCount = 0;
    for (int i = 0; i < totalOps; ++i) {
        bool isPut = static_cast<int>(gen() % 100) < putRatio;
        // 70% in hot set, 30% in cold set
        int key = (gen() % 100 < 70)
                    ? (gen() % hotKeys)
                    : (hotKeys + gen() % coldKeys);

        if (isPut) {
            cache.put(key, "val_" + std::to_string(key));
        } else {
            std::string result;
            getCount++;
            if (cache.get(key, result)) {
                hitCount++;
            }
        }
    }
    double hitRate = (getCount > 0) ? (100.0 * hitCount / getCount) : 0.0;
    std::cout << "GETs: " << getCount << ", Hits: " << hitCount
              << ", Hit Rate: " << std::fixed << std::setprecision(2)
              << hitRate << "%\n\n";
}

// Adaptive test: simulate hot migration (A/B two hot sets), observe ARC's p adjustment and hit rate change
void runArcAdaptiveTest(const std::string& testName,
                        int capacity,
                        int hotA, int hotB,   // Two hot sets size
                        int coldKeys,
                        int totalOps,
                        int putRatio,
                        int switchEvery) {     // Every how many operations switch hot set
    std::cout << "=== " << testName << " ===\n";
    std::random_device rd;
    std::mt19937 gen(rd());
    ArcCache<int, std::string> cache(static_cast<size_t>(capacity));

    auto pickKey = [&](bool useA) {
        // 70% select current hot set, 30% select cold data
        if (gen() % 100 < 70) {
            if (useA) {
                return gen() % hotA; // Hot A's key: 0..hotA-1
            } else {
                // Hot B in a non-overlapping interval: hotA..hotA+hotB-1
                return hotA + (gen() % hotB);
            }
        } else {
            // Cold data from another non-overlapping interval: hotA+hotB..hotA+hotB+coldKeys-1
            return hotA + hotB + (gen() % coldKeys);
        }
    };

    int getCount = 0, hitCount = 0;
    bool useA = true;
    for (int i = 0; i < totalOps; ++i) {
        if (switchEvery > 0 && i > 0 && (i % switchEvery == 0)) {
            useA = !useA; // Switch hot set
        }
        bool isPut = static_cast<int>(gen() % 100) < putRatio;
        int key = pickKey(useA);

        if (isPut) {
            cache.put(key, "val_" + std::to_string(key));
        } else {
            std::string result;
            getCount++;
            if (cache.get(key, result)) {
                hitCount++;
            }
        }
    }
    double hitRate = (getCount > 0) ? (100.0 * hitCount / getCount) : 0.0;
    std::cout << "GETs: " << getCount << ", Hits: " << hitCount
              << ", Hit Rate: " << std::fixed << std::setprecision(2)
              << hitRate << "%\n\n";
}

int main() {
    // —— ARC basic test (consistent with LFU template style) ——
    runArcTest("ARC Test 1: Baseline (CAPACITY=20, HOT_KEYS=20)",
               20, 20, 2000, 100000, 30);
    runArcTest("ARC Test 2: Increase Capacity (CAPACITY=40)",
               40, 20, 2000, 100000, 30);
    runArcTest("ARC Test 3: Reduce Hot Keys (HOT_KEYS=10)",
               20, 10, 2000, 100000, 30);
    runArcTest("ARC Test 4: High PUT rate (PUT=60%)",
               20, 20, 2000, 100000, 60);

    // —— ARC adaptive test: hot migration (trigger B1/B2 ghost hit) ——
    // Two hot sets of 20, 2000 cold data, 100000 total operations, switch hot set every 10000 operations
    runArcAdaptiveTest("ARC Adaptive Test 1: Workload Shift (switchEvery=10000)",
                       20, 20, 20, 2000, 100000, 30, 10000);
    runArcAdaptiveTest("ARC Adaptive Test 2: Faster Shift (switchEvery=5000)",
                       20, 20, 20, 2000, 100000, 30, 5000);
    runArcAdaptiveTest("ARC Adaptive Test 3: Larger Capacity (CAPACITY=40, switchEvery=10000)",
                       40, 20, 20, 2000, 100000, 30, 10000);
    runArcAdaptiveTest("ARC Adaptive Test 4: High PUT rate (PUT=60%, switchEvery=10000)",
                       20, 20, 20, 2000, 100000, 60, 10000);
    runArcAdaptiveTest("ARC Adaptive Test 5: Tighter Hotset (HOT_A=10, HOT_B=10)",
                       20, 10, 10, 2000, 100000, 30, 10000);

    return 0;
}