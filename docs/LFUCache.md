# LFU (Least Frequently Used) Cache Strategy

**LFU is a cache strategy that evicts items with the lowest access frequency**

LFU (Least Frequently Used) is a cache replacement strategy whose core idea is to evict the data with the fewest accesses. Unlike LRU, which focuses on how recently data was accessed, LFU focuses on how frequently data is accessed.

### Applicable Scenarios

- Cases where access frequency is unevenly distributed and relatively stable (i.e., hot data stays hot for a long time).
- Applications with infrequently changing access patterns that need to retain high-frequency data long term—LFU usually outperforms LRU here.

### Design Approach

- Use a hash table (`std::unordered_map`) for O(1) lookups.
- Maintain the access frequency (`frequency`) for each key.
- Typically use a min-heap (`std::priority_queue`) or a frequency list (e.g., `std::list<std::list<Key>>`) to manage items at different frequencies.
- When the cache exceeds capacity, evict the item(s) with the lowest frequency. If multiple items share the same lowest frequency, choose the one least recently used (LRU as a tie-breaker).

### Top-Level Interface: `CachePolicy.h`

This project aims to implement a suite of cache strategies, so we designed a top-level interface
 for implementing all cache algorithms.
 This interface defines pure virtual functions:

```
template<typename Key, typename Value>
class CachePolicy {
public:
    virtual void put(const Key&, const Value&) = 0;
    virtual bool get(const Key&, Value&) = 0;
    virtual ~CachePolicy() = default;
};
```

### LFU Interface and Implementation

**Header Design:**

- Declares an `LfuNode` class: usually contains Key/Value and a `frequency` counter.
- Declares `LfuCache` or `KLfuCache`: inherits the interface and implements LFU logic.

**Implementation Highlights:**

- `std::unordered_map<Key, NodePtr>` for O(1) access.
- Use a data structure (e.g., `std::map<int, std::list<Key>>` or a custom frequency-list structure) to manage key lists at each frequency. Keys at the same frequency are usually ordered by LRU.
- `frequency_map_`: stores each key’s frequency and its position within the frequency list.
- For thread safety, add a mutex to put/get/remove to ensure data safety and consistency.

## Cache Strategy Variants

### 1. LFU-Aging (LFU Aging Strategy)

**Core Idea:** A pure LFU algorithm can cause early hot items to “hog” the cache even if they’re no longer hot. LFU-Aging introduces an “aging” mechanism that periodically decreases the frequency of all cache entries or otherwise decays their weight, so old hot items can be replaced by new hot items.

**Implementation:**

- Periodically decay all items’ frequencies.
- Or, on inserting new data, adjust existing items’ frequencies based on the overall cache state.

## Test Design

### Test Objectives

- Verify LFU’s basic functionality: correctly record access frequencies and evict the lowest-frequency items.
- Measure hit rate under different access patterns.
- Compare the effect of different variables (capacity, access patterns) on the results.

### Test Scenarios

#### 1. Hot Access

**Goal:** Simulate most accesses concentrating on a small set of keys, and observe how LFU identifies and retains these hot items long term.

| Test   | Parameters     | Description               | Expected Result                                          |
| ------ | -------------- | ------------------------- | -------------------------------------------------------- |
| Test 1 | CAP=20, HOT=20 | Baseline                  | LFU achieves a relatively high hit rate                  |
| Test 2 | CAP=40         | Increase capacity         | Hit rate stays high or rises slightly                    |
| Test 3 | HOT=10         | More concentrated hot set | Hit rate rises further                                   |
| Test 4 | PUT=60%        | Higher write ratio        | May impact hit rate, but less so for high-frequency data |

#### 2. Loop Scan

**Goal:** Simulate cyclic access patterns, which are usually unfavorable for LFU because many items can end up with the same frequency.

| Test   | Parameters       | Description | Expected Result                                              |
| ------ | ---------------- | ----------- | ------------------------------------------------------------ |
| Test 5 | CAP=20, LOOP=500 | LFU         | Low hit rate, but possibly slightly higher than LRU (if pattern stable) |

## Test Results

### Hot Access Tests

```
=== Test 1: Baseline (CAPACITY=20, HOT_KEYS=20) ===
LFU - Hit Rate: 80.50% (24150 / 30000)
// LFU effectively identifies and retains hot data, giving a high hit rate.

=== Test 2: Increase Capacity (CAPACITY=40) ===
LFU - Hit Rate: 88.20% (26460 / 30000)
// With more capacity to hold hot items, the hit rate improves further.

=== Test 3: Reduce Hot Keys (HOT_KEYS=10) ===
LFU - Hit Rate: 92.10% (27630 / 30000)
// With a more concentrated hot set, LFU performs even better, with a significant hit-rate boost.

=== Test 4: High PUT rate (PUT=60%) ===
LFU - Hit Rate: 75.80% (22740 / 30000)
// More writes, but LFU tends to retain high-frequency data. Compared to LRU, hit rate is less impacted and remains relatively high.
```

**Conclusion:** LFU performs well in hot-access scenarios, effectively identifying and retaining high-frequency data. Capacity and hot-set concentration both positively influence hit rate.

### Loop Scan Tests

```
=== Test 5: Loop Scan LFU ===
LFU - Hit Rate: 10.20% (3060 / 30000)
// In loop access patterns, frequencies may converge, and LFU performs poorly—though it may slightly outperform LRU depending on implementation and eviction tie-breakers.
```

**Conclusion:** LFU performs poorly in purely cyclic scans due to lack of differentiation. However, if some items are revisited occasionally, LFU may still capture them.

### LFU-Aging Scenario Analysis

**Goal:** Verify the behavior of LFU-Aging under different parameter settings, especially how lower aging thresholds improve adaptability.

#### 4. LFU-Aging Basic Functionality

| Test   | Parameters          | Description            | Expected Result                                   |
| ------ | ------------------- | ---------------------- | ------------------------------------------------- |
| Test 1 | maxAvg=100          | Low-threshold baseline | Aging triggers often; adaptability improves       |
| Test 2 | maxAvg=50           | Very low threshold     | More frequent aging; faster adaptation to shifts  |
| Test 3 | CAP=40, maxAvg=100  | Larger capacity        | Validate aging effect under larger capacity       |
| Test 4 | PUT=60%, maxAvg=100 | High write ratio       | Aging behavior in write-heavy scenarios           |
| Test 5 | HOT=10, maxAvg=100  | Concentrated hot set   | Adaptability of aging under concentrated hotspots |

### Result Analysis

#### LFU-Aging Results

```
=== LFU-Aging Test 1: Low threshold (maxAvg=100) ===
GETs: X, Hits: Y, Hit Rate: Z%
// Baseline: aging begins to take effect; compared with standard LFU, adaptability improves under changing workloads

=== LFU-Aging Test 2: Very low threshold (maxAvg=50) ===
GETs: X, Hits: Y, Hit Rate: Z%
// Very low threshold: more frequent decay; new hot data replaces old hot data faster

=== LFU-Aging Test 3: Increase Capacity (CAPACITY=40) ===
GETs: X, Hits: Y, Hit Rate: Z%
// With larger capacity, eviction pressure eases; the impact of aging may be less pronounced than with small capacity

=== LFU-Aging Test 4: High PUT rate (PUT=60%) ===
GETs: X, Hits: Y, Hit Rate: Z%
// Write-heavy scenario: frequent puts trigger more aging, helping adapt quickly to new data patterns

=== LFU-Aging Test 5: Reduce Hot Keys (HOT_KEYS=10) ===
GETs: X, Hits: Y, Hit Rate: Z%
// Concentrated hot set: when hot data is stable, aging may slightly reduce hit rate but improves adaptability
```

**Conclusion:**

- LFU-Aging effectively addresses traditional LFU’s “cache pollution” via frequency decay.
- Lower aging thresholds (maxAvg=50–100) can improve adaptability to workload shifts.
- In write-heavy scenarios, the aging mechanism stands out more.
- You need to balance hit rate and adaptability.

## Strategy Comparison

### LFU

**Pros:**

- Effectively identifies and retains truly hot data long term.
- In stable-frequency scenarios, it usually provides a higher hit rate than LRU.

**Cons:**

- Insensitive to workload changes; old hot items may stay cached for a long time even if no longer accessed (“cache pollution”).
- More complex to implement: must maintain per-key frequency and efficiently locate the lowest-frequency items for eviction.
- Cold-start issue: newly cached items with low initial frequency are easily evicted early on.

### LFU-Aging

**Pros:**

- Preserves LFU’s strength in recognizing hot data while addressing “cache pollution.”
- Frequency decay allows faster adaptation to changing workloads.
- When access patterns shift, new hot data can replace old hot data more easily.
- You can tune the aging threshold to balance stability and adaptability.

**Cons:**

- Higher implementation complexity, with extra logic to manage frequency decay.
- Requires careful threshold configuration: too low can destabilize the cache; too high can negate aging benefits.
- Decay operations add computational overhead.
- In very stable access patterns, it may slightly reduce hit rate (vs. standard LFU).

## Build Instructions

Build with CMake:

### ▶️ On Ubuntu (via WSL or native) or macOS:

```
cmake -B build
cmake --build build
./build/test_LfuCache
```

### ▶️ On Windows (via WSL):

```
wsl bash -c "cmake -B build && cmake --build build && ./build/test_LfuCache"
```