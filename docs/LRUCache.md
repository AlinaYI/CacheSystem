# LRU (Least Recently Used) Cache Strategy

**LRU is one of the most common cache strategies: it evicts the item that was least recently accessed**

LRU (Least Recently Used) is the most common cache strategy. Its core idea is to evict the data item that has not been accessed for the longest time.

### Applicable Scenarios

- Workloads with obvious “hot data.”
- Access frequencies are relatively stable, with few short-term spikes.

### Design Approach

- Use a doubly linked list to maintain order and capacity.
- Place the most recently accessed item at the front; if capacity is exceeded, evict the node at the back.
- By maintaining recency order, aim to maximize `get` hit rate.

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
    virtual Value get(const Key&) = 0;
    virtual ~CachePolicy() = default;
};
```

### LRU Interface and Implementation

**Header Design:**

- `LruNode` node class: a doubly linked structure storing Key/Value.
- `KLruCache` class: inherits the interface and implements LRU logic.

**Implementation Highlights:**

- `std::unordered_map<Key, NodePtr>` provides O(1) access.
- A doubly linked list maintains access order, using `dummyHead`/`dummyTail` as sentinels.
- Use `std::weak_ptr` and `std::shared_ptr`:
  - `next_`: `shared_ptr` → owns the successor node.
  - `prev_`: `weak_ptr` → observes the predecessor (does not increase refcount, avoids cycles).
- For thread safety, add a mutex to put/get/remove operations to ensure data safety and consistency.

## Strategy Variants

### 1. KLruCache

**Core Idea:** Only items with at least **K** accesses are allowed into the main cache, preventing cold data from polluting the cache.

This is an improvement over LRU using two queues:

- **Cache queue:** stores hot data.
- **Access-history queue:** records access counts.

When data is accessed, first increment its count in the history queue. Only after the historical access count exceeds **K** is the data admitted into the cache queue, preventing cache pollution.

**Cache pollution:** Items with very few accesses remain in the cache after serving a request, wasting space.

**Notes:** The larger **K** is, the higher the potential hit rate, but eviction becomes harder (hot set is “stickier”).

### 2. HashLruCache (Lightweight LRU)

**Core Idea:** Split one large cache into multiple smaller shards, each maintaining its own independent LRU, so multiple threads accessing different keys don’t block each other on the same lock.

**Suitable For:** High-concurrency scenarios.

When heavy synchronized waiting increases latency, the solution is to reduce the critical section. Introduce hashing to lower lookup contention/time:

- Distribute cached data across **N** LruCache shards.
- On lookup, use the same hash to locate the shard and then query within that shard.
- Increase parallelism of LRU reads/writes and reduce time spent waiting on synchronization.

## Test Design

### Test Objectives

- Verify basic LRU functionality.
- Measure hit rate under different access patterns.
- Compare the impact of different variables (capacity, access patterns) on results.

### Test Scenarios

#### 1. Hot Access

**Goal:** Simulate most accesses concentrating on a small set of keys, such as popular pages or products.

| Test   | Parameters     | Description               | Expected Result        |
| ------ | -------------- | ------------------------- | ---------------------- |
| Test 1 | CAP=20, HOT=20 | Baseline                  | Medium LRU hit rate    |
| Test 2 | CAP=40         | Increase capacity         | Noticeable improvement |
| Test 3 | HOT=10         | More concentrated hot set | Further improvement    |
| Test 4 | PUT=60%        | Higher write ratio        | May reduce hit rate    |

#### 2. LRU-K vs. LRU

**Goal:** Verify whether LRU-K can better identify/cache hot data when hot items are frequently accessed.

| Test   | Parameters | Description             | Expected Result                                 |
| ------ | ---------- | ----------------------- | ----------------------------------------------- |
| Test 5 | k=2        | LRU-K vs. LRU           | LRU-K slightly higher hit rate                  |
| Test 6 | k=3        | Stricter promotion rule | May underperform LRU if promotion is too strict |

#### 3. Loop Scan

**Goal:** Simulate cyclic access (typical of sequential block reads) and observe how the cache policy copes.

| Test   | Parameters       | Description | Expected Result                           |
| ------ | ---------------- | ----------- | ----------------------------------------- |
| Test 7 | CAP=20, LOOP=500 | LRU         | Very low hit rate (frequent invalidation) |
| Test 8 | k=2              | LRU-K       | Limited or no improvement                 |

#### 4. Cold Data

**Goal:** Simulate every access being a new key—extremely unfavorable for any cache policy.

| Test    | Parameters          | Description | Expected Result   |
| ------- | ------------------- | ----------- | ----------------- |
| Test 9  | CAP=20, RANGE=10000 | LRU         | Hit rate nearly 0 |
| Test 10 | k=2                 | LRU-K       | Similar to LRU    |

#### 5. Hash-LRU Comparison

**Goal:** Verify whether a sharded cache improves outcomes under multithreaded or distributed access.

| Test    | Parameters | Description                   | Expected Result                                   |
| ------- | ---------- | ----------------------------- | ------------------------------------------------- |
| Test 11 | slice=1    | Hash-LRU baseline             | Similar to regular LRU                            |
| Test 12 | slice=4    | Enable multiple shards        | Slight improvement (decoupled access)             |
| Test 13 | LOOP       | Shards under sequential scan  | May be similar to regular LRU                     |
| Test 14 | Cold Data  | Sharding on all-cold workload | No obvious improvement; hit rate remains very low |

## Test Results

### Hot Access Tests

```
=== Test 1: Baseline (CAPACITY=20, HOT_KEYS=20) ===
GETs: 70146, Hits: 34511, Hit Rate: 49.20%
// Medium baseline hit rate: the hot set just fills the cache;
// frequent writes/evictions keep the cache unstable.

=== Test 2: Increase Capacity (CAPACITY=40) ===
GETs: 69905, Hits: 49012, Hit Rate: 70.11%
// Doubling capacity creates more room for hot keys; hit rate improves significantly.

=== Test 3: Reduce Hot Keys (HOT_KEYS=10) ===
GETs: 70218, Hits: 48889, Hit Rate: 69.62%
// Stronger concentration of hot items helps the cache converge faster;
// hit rate is close to Test 2.

=== Test 4: High PUT rate (PUT=60%) ===
GETs: 39931, Hits: 18036, Hit Rate: 45.17%
// More writes cause more frequent turnover; hit rate drops as expected.
```

**Conclusion:** In hot-access scenarios, overall hit rate correlates positively with parameters like capacity and hot-set concentration. LRU responds sensitively to these changes.

### LRU-K Tests

```
=== Test 5: LRU-K vs LRU (k=2) ===
GETs: 70221, Hits: 48595, Hit Rate: 69.20%
// Close to Tests 2 and 3, slightly lower than plain LRU,
// because two accesses are required before promotion into the cache.

=== Test 6: LRU-K (k=3) ===
GETs: 70264, Hits: 48957, Hit Rate: 69.68%
// Slightly higher than Test 5: hot keys are promoted more stably.
// It also suggests k=2 may promote a bit slowly; k=3 looks reasonable here.
```

**Conclusion:** Overall, LRU-K’s hit rate is not far from plain LRU. When the hot set is sufficiently concentrated, it works well—especially if hot keys get promoted quickly. Test 6 performed better than expected, suggesting very frequent hot-key accesses.

### Loop Scan Tests

```
=== Test 7: Loop Scan LRU ===
GETs: 79992, Hits: 0, Hit Rate: 0.00%
// Every access replaces a key; essentially no reuse.

=== Test 8: Loop Scan LRU-K (k=2) ===
GETs: 79971, Hits: 0, Hit Rate: 0.00%
// Cannot reach the promotion threshold, so it’s all misses.
```

**Conclusion:** Exactly as expected: cyclic access plus FIFO-like behavior makes the cache fail completely. This access pattern is a known weakness of LRU-family policies.

### Cold Data Tests

```
=== Test 9: All Cold Data LRU ===
GETs: 50000, Hits: 50000, Hit Rate: 100.00%
// Normally this should be all misses, but since each PUT is immediately followed by a GET,
// every value is hit. This is “write then immediate read,” not a true cold-scan workload.

=== Test 10: All Cold Data LRU-K (k=2) ===
GETs: 50000, Hits: 232, Hit Rate: 0.46%
// Unable to reach K accesses; nearly all misses—consistent with a true cold-data scenario.
```

**Conclusion:** The “100% hit rate” in Test 9 is due to test logic (PUT immediately followed by GET), not a true cold scan. It’s acceptable, but the documentation should note: “this is a write-then-immediate-read scenario, not pure cold misses.”

### Hash-LRU Tests

```
=== Test 11: Hash LRU (default slice) ===
GETs: 70208, Hits: 43176, Hit Rate: 61.50%
// Better than baseline LRU (Test 1), but slightly worse than LRU-K (Test 6).

=== Test 12: Hash LRU (4 slices) ===
GETs: 70212, Hits: 34233, Hit Rate: 48.76%
// Multiple shards should improve concurrency, but hit rate drops—likely due to uneven key distribution
// or per-shard capacity being too small.

=== Test 13: Loop Scan Hash LRU ===
GETs: 80223, Hits: 150, Hit Rate: 0.19%
// Slight improvement: sharding marginally slows eviction of keys, but it’s still a bad scenario.

=== Test 14: All Cold Data Hash LRU ===
GETs: 50000, Hits: 50000, Hit Rate: 100.00%
// Again indicates PUT-then-immediate-GET, leading to artificially high hit rate.
```

**Conclusion:** The `slice` setting of Hash-LRU must be chosen to match capacity; too many small shards can reduce local hit rates. Test 12 shows that more shards ≠ always better; you need a reasonable key distribution and per-shard capacity. For Cold Data, note that PUT-then-GET inflates hit rates and diverges from real-world cold scans.

## Strategy Comparison

### LRU

**Pros:**

- Simple to implement.
- Stable hit rate.

**Cons:**

- Vulnerable to cold data and sequential scans.
- Lacks a “filtering” capability.

### LRU-K

**Pros:**

- Effectively filters one-time accesses.
- Improves hot-item hit rates.

**Cons:**

- “Cold start” in the initialization phase.
- More complex to implement.
- Not ideal for latency-sensitive applications.

### Hash-LRU

**Pros:**

- Naturally suits high-concurrency, multi-tenant environments.
- Prevents hotspot contention.

**Cons:**

- Hit rate depends on key distribution.
- Too many shards reduce per-shard capacity and efficiency.

## Build Instructions

Build with CMake:

### ▶️ On Ubuntu (via WSL or native) or macOS:

```
cmake -B build
cmake --build build
./build/test_LruOnly
```

### ▶️ On Windows (via WSL):

```
wsl bash -c "cmake -B build && cmake --build build && ./build/test_LruOnly"
```