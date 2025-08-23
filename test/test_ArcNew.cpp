#include <iostream>
#include <string>
#include <random>
#include <iomanip>
#include "Arc_new.h"   // 确保包含路径正确，例如: #include "../include/Arc_new.h"

using namespace Cache;

// 70% 从热点集合选，30% 从冷集合选
static int pickHotColdKey(int hotKeys, int coldKeys, std::mt19937& gen) {
    std::uniform_int_distribution<int> coin(0, 99);
    if (coin(gen) < 70) {
        std::uniform_int_distribution<int> d(0, hotKeys - 1);
        return d(gen);
    } else {
        std::uniform_int_distribution<int> d(0, coldKeys - 1);
        return hotKeys + d(gen);
    }
}

// —— 基础测试：热点/冷数据 + PUT/GET 混合 —— //
void runArcNewTest(const std::string& testName,
                   int capacity, int hotKeys, int coldKeys,
                   int totalOps, int putRatio) {
    std::cout << "=== " << testName << " ===\n";
    std::random_device rd;
    std::mt19937 gen(rd());
    Arc_new<int, std::string> cache(static_cast<size_t>(capacity));

    std::uniform_int_distribution<int> coin(0, 99);

    int getCount = 0, hitCount = 0;
    for (int i = 0; i < totalOps; ++i) {
        bool isPut = (coin(gen) < putRatio);
        int key = pickHotColdKey(hotKeys, coldKeys, gen);

        if (isPut) {
            cache.put(key, "val_" + std::to_string(key));
        } else {
            std::string result;
            ++getCount;
            if (cache.get(key, result)) ++hitCount;
        }
    }
    double hitRate = (getCount > 0) ? (100.0 * hitCount / getCount) : 0.0;
    std::cout << "GETs: " << getCount
              << ", Hits: " << hitCount
              << ", Hit Rate: " << std::fixed << std::setprecision(2)
              << hitRate << "%\n\n";
}

// —— 自适应测试：热点迁移（A/B 两批热点轮换）—— //
void runArcNewAdaptiveTest(const std::string& testName,
                           int capacity,
                           int hotA, int hotB,
                           int coldKeys,
                           int totalOps,
                           int putRatio,
                           int switchEvery) {
    std::cout << "=== " << testName << " ===\n";
    std::random_device rd;
    std::mt19937 gen(rd());
    Arc_new<int, std::string> cache(static_cast<size_t>(capacity));

    std::uniform_int_distribution<int> coin(0, 99);
    auto pickKey = [&](bool useA) {
        if (coin(gen) < 70) {
            if (useA) {
                std::uniform_int_distribution<int> dA(0, hotA - 1);
                return dA(gen);
            } else {
                std::uniform_int_distribution<int> dB(0, hotB - 1);
                return hotA + dB(gen);
            }
        } else {
            std::uniform_int_distribution<int> dC(0, coldKeys - 1);
            return hotA + hotB + dC(gen);
        }
    };

    int getCount = 0, hitCount = 0;
    bool useA = true;
    for (int i = 0; i < totalOps; ++i) {
        if (switchEvery > 0 && i > 0 && (i % switchEvery == 0)) {
            useA = !useA; // 切换热点集合
        }
        bool isPut = (coin(gen) < putRatio);
        int key = pickKey(useA);

        if (isPut) {
            cache.put(key, "val_" + std::to_string(key));
        } else {
            std::string result;
            ++getCount;
            if (cache.get(key, result)) ++hitCount;
        }
    }
    double hitRate = (getCount > 0) ? (100.0 * hitCount / getCount) : 0.0;
    std::cout << "GETs: " << getCount
              << ", Hits: " << hitCount
              << ", Hit Rate: " << std::fixed << std::setprecision(2)
              << hitRate << "%\n\n";
}

// —— Small demo: stable reproduce B1 ghost hit, and print p() change —— //
// Note: standard ARC ghost doesn't store value, so get() hit B1 will return false;
//       we then put the key back with value (into T2).
void runArcNewGhostB1Demo() {
    std::cout << "=== Arc_new Ghost Demo (B1 Hit) ===\n";
    Arc_new<int, std::string> cache(/*capacity*/3);

    // Step1: 只用 put 填满 T1（避免提前升入 T2）
    cache.put(0, "v0");
    cache.put(1, "v1");
    cache.put(2, "v2");
    std::cout << "Initial p = " << cache.p() << "\n";

    // Step2: new put 3, trigger replace, evict one from T1 to B1 (usually the LRU of 0/1/2)
    cache.put(3, "v3");

    // Assume the LRU is 2 (sequence depends on implementation/access order, this demo usually evicts 2 to B1)
    // To ensure hit, we don't do complex recognition, directly access 2:
    std::string tmp;
    bool hit = cache.get(2, tmp); // Expected false (hit B1: increase p, replace returns false)
    std::cout << "get(2) hit? " << (hit ? "true" : "false") << ", p(after B1 hit) = " << cache.p() << "\n";

    // Step3: after miss, restore the key (with value), will enter T2 MRU
    cache.put(2, "v2");

    // Access again, should hit (now in T2)
    hit = cache.get(2, tmp);
    std::cout << "get(2) after put -> hit? " << (hit ? "true" : "false") << "\n\n";
}

int main() {
    // —— Basic test —— //
    runArcNewTest("Arc_new Test 1: Baseline (CAPACITY=20, HOT_KEYS=20)",
                  20, 20, 2000, 100000, 30);
    runArcNewTest("Arc_new Test 2: Increase Capacity (CAPACITY=40)",
                  40, 20, 2000, 100000, 30);
    runArcNewTest("Arc_new Test 3: Reduce Hot Keys (HOT_KEYS=10)",
                  20, 10, 2000, 100000, 30);
    runArcNewTest("Arc_new Test 4: High PUT rate (PUT=60%)",
                  20, 20, 2000, 100000, 60);

    // —— Adaptive test (hot migration) —— //
    runArcNewAdaptiveTest("Arc_new Adaptive 1: switchEvery=10000",
                          20, 20, 20, 2000, 100000, 30, 10000);
    runArcNewAdaptiveTest("Arc_new Adaptive 2: Faster shift (5000)",
                          20, 20, 20, 2000, 100000, 30, 5000);
    runArcNewAdaptiveTest("Arc_new Adaptive 3: Larger CAP=40",
                          40, 20, 20, 2000, 100000, 30, 10000);
    runArcNewAdaptiveTest("Arc_new Adaptive 4: PUT=60%",
                          20, 20, 20, 2000, 100000, 60, 10000);
    runArcNewAdaptiveTest("Arc_new Adaptive 5: tighter hotset (10/10)",
                          20, 10, 10, 2000, 100000, 30, 10000);

    // —— B1 ghost hit demo —— //
    runArcNewGhostB1Demo();

    return 0;
}