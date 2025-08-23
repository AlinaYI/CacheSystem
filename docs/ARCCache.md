# ARC (Adaptive Replacement Cache) Strategy

**ARC is a cache algorithm that adaptively balances recency and frequency**

ARC (Adaptive Replacement Cache) is a cache replacement algorithm that combines the strengths of LRU (Least Recently Used) and LFU (Least Frequently Used). By adapting its strategy, ARC delivers good hit rates across different access patterns and aims to overcome the limitations of LRU and LFU under specific workloads.

### Applicable Scenarios

- Workloads with varying access patterns, mixing hot data, sequential scans, and bursty accesses.
- Scenarios that need to consider both temporal locality (recent access) and frequency locality (frequent access).
- Applications that aim to maintain high hit rates across a wide range of workloads.

### Design Approach

ARC manages cache entries by maintaining four lists and a dynamic parameter `p`:

- **T1 (Temporal List 1):** Stores items that have been accessed **once**. This behaves like an LRU list to capture short-term hot items.
- **B1 (Ghost List 1):** Records keys that used to be in T1 but have been evicted. This “ghost list” helps decide whether to increase T2’s capacity (i.e., decrease T1’s capacity).
- **T2 (Temporal List 2):** Stores items that have been accessed **multiple** times. This also behaves like an LRU list to capture long-term hot items.
- **B2 (Ghost List 2):** Records keys that used to be in T2 but have been evicted. Similarly, this ghost list helps decide whether to increase T1’s capacity (i.e., decrease T2’s capacity).

**Dynamic parameter `p`:**
 `p` is dynamically adjusted within `0` to `capacity_`, representing the target size of T1. The adjustment of `p` decides whether ARC leans more toward LRU behavior (increase T1) or LFU-like behavior (increase T2).

### Top-Level Interface: `CachePolicy.h`

This project aims to implement a collection of cache strategies, so a top-level interface is designed for implementing various cache algorithms. This interface defines the following pure virtual functions:

```
template<typename Key, typename Value>
class CachePolicy {
public:
    virtual void put(const Key&, const Value&) = 0;
    virtual bool get(const Key&, Value&) = 0;
    virtual ~CachePolicy() = default;
};
```

### ARC Interface and Implementation

**Header design:**

- Declares the `ArcCache` class, which inherits from `CachePolicy` and implements ARC logic.
- Internal data structures include `t1_`, `b1_`, `t2_`, `b2_` (implemented as `std::list` doubly linked lists), and `cache_map_` (`std::unordered_map`) for fast lookup.
- `CacheEntry` struct: stores the value, the iterator in the corresponding list, and the list type (`ListType`).
- `b1_map_` and `b2_map_`: assist `b1_` and `b2_` with O(1) lookups by mapping keys to their iterators in the ghost lists.

**Implementation highlights:**

- `put(const Key& key, const Value& value)`: handles cache hits, ghost hits (B1 or B2), and new insertions; adjusts `p` and performs replacement according to ARC.
- `get(const Key& key, Value& value)`: handles cache hits and moves the hit item to T2.
- `replace(bool hit_in_b1)`: core replacement logic; based on `p` and whether the hit was in B1, decides to evict from T1 or T2 and insert into the corresponding ghost list.
- `moveToT2(const Key& key)`: moves a key from T1 or T2 to the MRU end of T2.
- `evictFromList(std::list<Key>& list, ListType type)`: evicts the LRU item from the specified list and adds its key to the corresponding ghost list and map.
- To ensure thread safety, `mutex` is used in put/get/remove operations to guarantee data safety and consistency.

## Test Design

### Test Objectives

- Verify ARC’s basic functionality: `put`, `get`, `clear`, `size`, `capacity`.
- Evaluate ARC’s adaptivity and hit-rate performance under different access patterns (hot set, loops, workload shifts).
- Verify that the dynamic adjustment of the `p` parameter matches expectations.
- Test ghost-list hits (B1, B2) and their impact on cache behavior.

### Test Scenarios

#### 1. Hot Access

**Goal:** Simulate most accesses focusing on a small set of keys and observe how ARC leverages its dual paths (T1/T2) to retain hot data.

| Test   | Parameters     | Description               | Expected Result                          |
| ------ | -------------- | ------------------------- | ---------------------------------------- |
| Test 1 | CAP=20, HOT=20 | Baseline                  | Higher hit rate than LRU and LFU         |
| Test 2 | CAP=40         | Larger capacity           | Significant improvement, remains high    |
| Test 3 | HOT=10         | More concentrated hot set | Further increase, stable at a high level |

#### 2. Loop Scan

**Goal:** Simulate cyclic access patterns, typically unfavorable for LRU and LFU. Observe how ARC handles this pattern and leverages B1/B2 to avoid frequent evictions.

| Test   | Parameters       | Description | Expected Result                                              |
| ------ | ---------------- | ----------- | ------------------------------------------------------------ |
| Test 4 | CAP=20, LOOP=500 | ARC         | Hit rate may be slightly higher than LRU, but still low overall |

#### 3. Workload Shift Simulation

**Goal:** Simulate a shift in the hot set from one group of keys to another and verify ARC’s adaptivity.

| Test   | Parameters     | Description       | Expected Result                                              |
| ------ | -------------- | ----------------- | ------------------------------------------------------------ |
| Test 5 | Shift Workload | Hot set migration | ARC quickly adapts to the new hot set; hit rate dips briefly then recovers |

#### 4. Ghost List Hits

**Goal:** Specifically test how ARC uses B1 and B2 when evicted data is accessed again, adjusting policy and bringing items back into the cache.

| Test   | Scenario | Expected Result                                           |
| ------ | -------- | --------------------------------------------------------- |
| Test 6 | B1 Hit   | Increase `p`; item moves from B1 to T2; hit rate improves |
| Test 7 | B2 Hit   | Decrease `p`; item moves from B2 to T2; hit rate improves |

## Test Results (Examples; actual values may vary due to implementation details and randomness)

### Hot Access Tests

```
=== Test 1: Baseline (CAPACITY=20, HOT_KEYS=20) ===
ARC - Hit Rate: 85.00% (25500 / 30000)
// ARC outperforms the LRU/LFU baselines and balances effectively.

=== Test 2: Increase Capacity (CAPACITY=40) ===
ARC - Hit Rate: 90.50% (27150 / 30000)
// Increasing capacity yields a significant improvement; ARC makes good use of the larger space.

=== Test 3: Reduce Hot Keys (HOT_KEYS=10) ===
ARC - Hit Rate: 93.20% (27960 / 30000)
// With a more concentrated hot set, ARC’s adaptivity lets it focus better on core hot items.
```

**Conclusion:** In hot-access scenarios, ARC performs very well. Its adaptivity enables efficient use of cache space, and hit rates further increase as the hot-set concentration and capacity grow.

### Loop Scan Tests

```
=== Test 4: Loop Scan ARC ===
ARC - Hit Rate: 25.00% (7500 / 30000)
// Compared to LRU’s 0%, ARC improves by recognizing second accesses (moving into T2), though overall remains low.
```

**Conclusion:** ARC outperforms plain LRU in loop-scan scenarios but still cannot fully address this pattern, as most data is accessed only once.

### Workload Shift Tests

```
=== Test 5: Workload Shift ARC ===
ARC - Hit Rate: 78.00% (23400 / 30000)
// ARC can quickly detect workload changes and dynamically adjust p, adapting to the new hot set. After a brief dip, the hit rate quickly recovers and remains high.
```

**Conclusion:** ARC’s adaptivity allows it to perform well under workload changes and quickly adjust strategies to the new access pattern.

## Strategy Comparison

### ARC

**Pros:**

- **Strong adaptivity:** Dynamically adjusts the policy based on workload, outperforming static LRU and LFU.
- **Balances temporal/frequency locality:** Considers both recent access and access frequency, suitable for complex scenarios.
- **Ghost lists:** By recording access info for evicted items, it intelligently decides whether to recache or adjust policy, effectively avoiding “cache pollution” and “thrashing”.

**Cons:**

- **Implementation complexity:** Compared to LRU or LFU, ARC maintains more data structures (four lists and dynamic parameter `p`), making it more complex to implement.
- **Overhead:** Maintaining multiple lists and `p` introduces extra memory and CPU overhead.
- **Understanding & debugging:** The internal logic is more complex, making it harder to understand and debug.

## Build Instructions

Build with CMake:

### ▶️ On Ubuntu (via WSL or native) or macOS:

```
cmake -B build
cmake --build build
./build/test_ArcCache
```

### ▶️ On Windows (via WSL):

```
wsl bash -c "cd /home/alina/CacheSystem && cmake -B build && cmake --build build && ./build/test_ArcCache"
```

### ▶️ On macOS

```
cmake -B build
cmake --build build
./build/test_ArcCache
```
