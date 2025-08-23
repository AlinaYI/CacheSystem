# CacheSystem

This project implements multiple cache replacement algorithms in C++ (e.g., **LRU**, **LFU**, **ARC**, **LRU-K**, **LFU-Aging**) and provides benchmark scenarios to evaluate hit-rate performance under various workloads.

> Recommended platforms: **Linux (Ubuntu 22.04 / WSL)** or **macOS**.  
> Windows users should use **WSL** for a consistent toolchain and better developer experience.

---

## Features

- **Unified interface**: `CachePolicy.h`
- **Policies implemented**
  - **LRU** (Least Recently Used)
  - **LFU** (Least Frequently Used, with an **aging** parameter)
  - **ARC** (standard ARC with ghost lists **B1/B2** and adaptive knob **p**)
  - **Arc_new** (this repo’s standard ARC; fixes the ghost-hit ordering pitfall: **remove ghost → adjust p → replace**)
  - **KArc** (engineering split: independent **LRU/LFU partitions** + adaptive capacity allocation)
- **Benchmarks**: hot/cold mix, **workload shift**, PUT/GET ratio variations, etc.
- **One-click script**: build → run all tests → aggregate hit rates (terminal table + `summary.csv`)

---

## Algorithm Overview

| Algorithm | Core Idea | Pros | Limitations |
| --- | --- | --- | --- |
| **LRU** | Recency: evict least recently used | Simple, low overhead | Degrades on cyclic scans & bursty workloads |
| **LFU** | Frequency: evict lowest access freq | Retains long-term hot items | Slow to adapt to hot-set shifts; needs aging |
| **ARC** | T1/T2 + B1/B2 ghosts; adaptive knob `p` | Balances recency & frequency adaptively | More complex; maintains 4 lists |
| **Arc_new** | Our standard ARC implementation | Fixes iterator invalidation on ghost hits (remove ghost → adjust p → replace) | Ghosts store no values; misses require upper-layer `put` |
| **KArc** | LRU/LFU partitions with top-level scheduler | Engineering-friendly; easy to swap inner strategies | Larger codebase; higher learning curve |

---

## Project Structure

```text
CacheSystem/
├─ include/
│  ├─ CachePolicy.h           # Unified policy interface
│  ├─ LruCache.h / .tpp       # LRU
│  ├─ LfuCache.h / .tpp       # LFU
│  ├─ ArcCache.h / .tpp       # ARC (earlier version)
│  ├─ Arc_new.h / .tpp        # Standard ARC (recommended for comparison)
│  ├─ KArcCache.h             # KArc top-level scheduler
│  ├─ KArcCacheNode.h         # KArc node definition
│  ├─ KArcLruPart.h           # KArc LRU partition
│  ├─ KArcLfuPart.h           # KArc LFU partition
│  └─ ...
├─ src/                       # Non-template .cpp files (if any)
├─ test/
│  ├─ test_LruOnly.cpp
│  ├─ test_LfuCache.cpp
│  ├─ test_ArcCache.cpp
│  ├─ test_ArcNew.cpp
│  └─ ...
├─ CMakeLists.txt
├─ run_all_tests.sh           # One-click build & summary script
└─ README.md
```



## Environment Setup
Ubuntu / WSL (recommended)

```bash
sudo apt update
sudo apt install -y cmake g++ build-essential libgtest-dev
```



 macOS

```bash
xcode-select --install
brew install cmake
brew install googletest   # if you need GTest
```



Build & Run

```bash
# Go to repo root
cd ~/CacheSystem

# Configure & build (Release)
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j

# Run individual tests
./build/test_LruOnly
./build/test_LfuCache
./build/test_ArcCache
./build/test_ArcNew
```



Each test prints GET hit rates for several scenarios, e.g.:

```bash
=== Arc_new Test 1: Baseline (CAPACITY=20, HOT_KEYS=20) ===
GETs: 70000, Hits: 61234, Hit Rate: 87.48%
```



## Run All Tests & Compare

The script `run_all_tests.sh` will:

- configure & build via CMake
- execute: `test_LruOnly`, `test_LfuCache`, `test_ArcCache`, `test_ArcNew`
- write raw logs to `build/logs/*.log`
- parse lines like `=== Scenario ===` and `Hit Rate: xx.xx%`
- print a comparison table and generate `build/logs/summary.csv`

```bash
chmod +x run_all_tests.sh
./run_all_tests.sh
```



### Sample Output

```bash
📊 Comparison (higher is better)

Scenario                                            | LRU    | LFU    | ARC    | ARC_new
----------------------------------------------------------------------------------------
Baseline (CAPACITY=20, HOT_KEYS=20)                 | 75.12% | 81.45% | 86.90% | 87.32%
Increase Capacity (CAPACITY=40)                     | 80.50% | 86.80% | 90.10% | 90.42%
Reduce Hot Keys (HOT_KEYS=10)                       | 82.34% | 88.07% | 92.55% | 92.80%
High PUT rate (PUT=60%)                             | 70.20% | 76.40% | 80.30% | 80.50%
Workload Shift (switchEvery=10000, CAPACITY=20)     | 60.10% | 68.25% | 76.00% | 76.40%

📁 CSV saved: build/logs/summary.csv

🏆 Winners per scenario:
  - Baseline (CAPACITY=20, HOT_KEYS=20): ARC_new @ 87.32%
  ...
```



## Run on WSL

```
wsl ~
cd ~/CacheSystem
chmod +x run_all_tests.sh
./run_all_tests.sh
```

If you see `bad interpreter: ^M`, your script has Windows line endings (CRLF). Fix it with:

```
sed -i 's/\r$//' run_all_tests.sh
# or:
sudo apt install -y dos2unix && dos2unix run_all_tests.sh
```

------

## Enable AddressSanitizer (debug memory bugs)

Use ASan to quickly locate out-of-bounds and use-after-free issues (e.g., ARC ghost-hit ordering):

```
cmake -B build -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS="-fsanitize=address -fno-omit-frame-pointer -O0 -g"
cmake --build build -j
ASAN_OPTIONS=halt_on_error=1 ./build/test_ArcNew
```

------

## How to Add a New Policy

1. In `include/`, add `YourCache.h/.tpp`, inherit `CachePolicy<Key, Value>`, and implement:

   ```
   void  put(const Key&, const Value&) override;
   bool  get(const Key&, Value&) override;
   Value get(const Key&) override;
   ```

2. Add `test/test_YourCache.cpp`, reusing the existing test templates (hot/cold mix, adaptive workload).

3. Update `CMakeLists.txt` to create the executable and link GTest.

4. (Optional) Add the new executable & alias to `run_all_tests.sh` to appear in the summary.

------



## Author

**Alina Ding**
 Email: `yding384@gmail.com`
 LinkedIn: https://www.linkedin.com/in/yidingalina/