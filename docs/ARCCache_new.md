# NewArc (ARC-inspired, Engineering-oriented) Cache Policy

**NewARC is an engineering-oriented cache algorithm combining LRU and LFU**

NewARC is a cache policy inspired by ARC. By partitioning into LRU (Least Recently Used) and LFU (Least Frequently Used), and combining ghost caches with a threshold-based promotion mechanism, it maintains a high hit rate under diverse access patterns. Compared with standard ARC, KArc is more aligned with engineering application scenarios, with intuitive logic and precise capture of long-term hot items.

### Applicable Scenarios
- Data access patterns that contain both short-term and long-term hot items.
- Need to avoid “scan pollution” of pure LRU while overcoming the “historical baggage” of LFU.
- Engineering systems (databases, distributed storage, KV caches) that require explainable and tunable policies.

### Design Concept
The NewARC algorithm manages the cache by maintaining an LRU part, an LFU part, and their respective ghost lists:

- **LRU Part**: Stores *recently accessed* items. Each node carries an access count `accessCount`.
- **LRU Ghost**: Records keys most recently evicted from LRU.
- **LFU Part**: Stores *long-term frequently accessed* items, using `map<frequency, list<nodes>>` for precise management.
- **LFU Ghost**: Records keys most recently evicted from LFU.

**Capacity Adaptation Mechanism:**
If a key hits in the LRU Ghost → recency matters more → give LRU +1 capacity and subtract 1 from LFU.  
If a key hits in the LFU Ghost → frequency matters more → give LFU +1 capacity and subtract 1 from LRU.

### Top-level Interface: `CachePolicy.h`
This project aims to implement a collection of cache strategies, so we designed a top-level interface
for implementing all cache algorithms.
This interface defines pure virtual functions:

```cpp
template<typename Key, typename Value>
class CachePolicy {
public:
    virtual void put(const Key&, const Value&) = 0;
    virtual bool get(const Key&, Value&) = 0;
    virtual ~CachePolicy() = default;
};
```



### NewARC Interface and Implementation

**Header Design:**

- `ArcLruPart`: Implements the LRU part, supports node promotion, and maintains the LRU Ghost.
- `ArcLfuPart`: Implements the LFU part, supports frequency buckets, and maintains the LFU Ghost.
- `NewArcCache`: Top-level class that composes the LRU and LFU parts and handles adaptive capacity adjustments.

**Implementation Highlights:**

- `put(const Key& key, const Value& value)`: Write to LRU first; if a node meets the promotion condition, promote it to LFU; if a ghost is hit, trigger capacity adjustment.
- `get(const Key& key, Value& value)`: Lookup in LRU → if promotion condition is met, promote to LFU; otherwise look up in LFU.
- `checkGhostCaches(Key)`: Detect hits in ghost caches and trigger LRU/LFU capacity increases/decreases.

## Test Design

### Test Objectives

- Verify NewArc’s basic functions: put, get, promotion logic, and capacity adjustment.
- Evaluate NewArc’s hit-rate performance under different access patterns.

### Test Scenarios

#### 1. Hot Access Scenario

**Goal:** Simulate most accesses concentrating on a few keys and observe how NewArc uses promotion to move hot items into LFU.

| Test   | Parameters     | Description               | Expected Result                        |
| ------ | -------------- | ------------------------- | -------------------------------------- |
| Test 1 | CAP=20, HOT=20 | Baseline                  | Higher hit rate than LRU & LFU         |
| Test 2 | CAP=40         | Larger capacity           | Significant improvement, stays high    |
| Test 3 | HOT=10         | More concentrated hot set | Further increase, stable at high level |

#### 2. Loop Scan Scenario

**Goal:** Simulate cyclic data access and test whether ghost caches can avoid frequent mistaken evictions.

| Test   | Parameters       | Description | Expected Result                                           |
| ------ | ---------------- | ----------- | --------------------------------------------------------- |
| Test 4 | CAP=20, LOOP=500 | Loop access | Higher hit rate than LRU, close to ARC, overall still low |

#### 3. Workload Shift Simulation

**Goal:** Simulate a workload shift from one hot set to another and verify ARC’s adaptivity.

| Test   | Parameters     | Description         | Expected Result                                              |
| ------ | -------------- | ------------------- | ------------------------------------------------------------ |
| Test 5 | Shift Workload | Hot set transitions | NewARC adapts quickly to new hot items; hit rate dips briefly then recovers |

#### 4. Ghost List Hits

**Goal:** Specifically test NewARC’s capacity adjustment logic.

| Test   | Scenario  | Expected Result                                             |
| ------ | --------- | ----------------------------------------------------------- |
| Test 6 | LRU Ghost | LRU +1, LFU -1 → improves short-term hit rate               |
| Test 7 | LFU Ghost | LFU +1, LRU -1 → improves stability for long-term hot items |



## Build Instructions

Build with CMake:

### ▶️ On Ubuntu (via WSL or native) or macOS:

```
cmake -B build
cmake --build build
./build/test_ArcCache
```

▶️ On Windows (via WSL):

```
wsl bash -c "cd /home/alina/CacheSystem && cmake -B build && cmake --build build && ./build/test_ArcCache"
```

▶️ On macOS

```
cmake -B build
cmake --build build
./build/test_ArcCache
```
