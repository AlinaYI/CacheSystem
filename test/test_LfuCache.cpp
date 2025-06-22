#include <iostream>
#include <string>
#include <random>
#include <iomanip>
#include "LfuCache.h"

using namespace Cache;

void runLfuTest(const std::string& testName, int capacity, int hotKeys, int coldKeys, int totalOps, int putRatio, int maxAverage = 1000000) {
    std::cout << "=== " << testName << " ===\n";
    std::random_device rd;
    std::mt19937 gen(rd());
    LfuCache<int, std::string> cache(capacity, maxAverage);

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
    std::cout << "GETs: " << getCount << ", Hits: " << hitCount
              << ", Hit Rate: " << std::fixed << std::setprecision(2)
              << (100.0 * hitCount / getCount) << "%\n\n";
}

int main() {
    runLfuTest("Lfu Test 1: Baseline (CAPACITY=20, HOT_KEYS=20)", 20, 20, 2000, 100000, 30);
    runLfuTest("Lfu Test 2: Increase Capacity (CAPACITY=40)", 40, 20, 2000, 100000, 30);
    runLfuTest("Lfu Test 3: Reduce Hot Keys (HOT_KEYS=10)", 20, 10, 2000, 100000, 30);
    runLfuTest("Lfu Test 4: High PUT rate (PUT=60%)", 20, 20, 2000, 100000, 60);
    runLfuTest("Lfu Test 5: Lower maxAvgFreq", 20, 20, 2000, 100000, 30, 500);
    return 0;
}
