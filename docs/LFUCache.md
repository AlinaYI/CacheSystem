# LFU (Least Frequently Used) 缓存策略

## LFU 是一种淘汰访问频率最低的数据的缓存策略

LFU（Least Frequently Used）是一种缓存替换策略，其核心思想是淘汰掉访问次数最少的数据。与 LRU 关注数据最近被访问的时间不同，LFU 更关注数据被访问的频率。

### 适用场景
- 数据访问频率分布不均匀，且频率相对稳定（即热点数据长期保持热度）的情况。
- 对于那些访问模式变化不频繁，需要长期保留高频访问数据的应用，LFU 表现通常优于 LRU。

### 设计思路
- 使用哈希表（`std::unordered_map`）实现 O(1) 的数据查找。
- 维护每个 Key 的访问频率（`frequency`）。
- 通常使用一个最小堆（`std::priority_queue`）或一个频率链表（如 `std::list<std::list<Key>>`）来管理不同频率的数据。
- 当缓存容量超限时，淘汰访问频率最低的数据。如果存在多个相同最低频率的数据，通常会选择其中最久未被使用的（LRU 策略作为辅助）。

### 顶层接口： `CachePolicy.h`
这个项目是想写所有的缓存策略的合集，那么这里就设计了一个顶层的接口
用于实现所有的缓存算法的
这个接口就包括定义了pure virtual function

```cpp
template<typename Key, typename Value>
class CachePolicy {
public:
    virtual void put(const Key&, const Value&) = 0;
    virtual bool get(const Key&, Value&) = 0;
    virtual ~CachePolicy() = default;
};
```

### LFU 接口和实现

**头文件设计：**
- 声明了 `LfuNode` 节点类：通常包含 Key/Value 和 `frequency` 计数器。
- 声明了 `LfuCache` 或 `KLfuCache` 类：继承接口，实现 LFU 逻辑。

**实现文件特点：**
- `std::unordered_map<Key, NodePtr>` 实现 O(1) 访问。
- 使用一个数据结构（例如，`std::map<int, std::list<Key>>` 或者自定义的频率链表结构）来管理不同频率的 Key 列表。每个频率级别下的 Key 列表通常按 LRU 顺序排列。
- `frequency_map_`：存储每个 Key 对应的频率及其在频率链表中的位置。
- 为确保多线程安全性，在 put/get/remove 操作中加入 mutex，确保数据安全性和一致性。

## 缓存策略变体

### 1. LFU-Aging (LFU 老化策略)

**核心思想：** 纯粹的 LFU 算法可能导致早期访问频繁的数据“霸占”缓存，即使它们现在已经不再热门。LFU-Aging 引入了“老化”机制，定期降低所有缓存项的访问频率，或以其他方式衰减其权重，使得老的热点数据能够被新的热点数据淘汰。

**实现：**
- 定期对所有缓存项的频率进行衰减操作。
- 或在插入新数据时，根据缓存的整体情况调整已有数据的频率。

## 测试设计

### 测试目标
- 验证 LFU 的基本功能：正确记录访问频率，淘汰频率最低的数据。
- 测试命中率在不同访问模式下的表现。
- 对比不同变量（capacity、访问模式）对结果的影响。

### 测试场景

#### 1. 热点访问场景 (Hot Access)
**目的：** 模拟大部分访问集中在少数 key 上，并观察 LFU 如何识别并长期保留这些热点数据。

| 测试   | 参数            | 说明           | 预期结果             |
|--------|-----------------|----------------|----------------------|
| Test 1 | CAP=20, HOT=20  | 基线测试        | LFU 命中率较高      |
| Test 2 | CAP=40          | 增大缓存容量    | 命中率保持高位或略升 |
| Test 3 | HOT=10          | 热点更集中      | 命中率进一步上升     |
| Test 4 | PUT=60%         | 写操作比例增加  | 可能会影响命中率，但对高频数据影响较小 |

#### 2. 循环扫描场景 (Loop Scan)
**目的：** 模拟数据循环访问场景，通常对 LFU 不利，因为所有数据访问频率都可能相同。

| 测试   | 参数             | 说明     | 预期结果                 |
|--------|------------------|----------|--------------------------|
| Test 5 | CAP=20, LOOP=500 | LFU      | 命中率较低，但可能略高于 LRU （如果访问模式稳定）|

## 测试结果

### 热点访问场景测试

```
=== Test 1: Baseline (CAPACITY=20, HOT_KEYS=20) ===
LFU - 命中率: 80.50% (24150 / 30000)
// LFU 能很好地识别并保留热点数据，命中率较高。

=== Test 2: Increase Capacity (CAPACITY=40) ===
LFU - 命中率: 88.20% (26460 / 30000)
// 容量增大，能容纳更多热点，命中率进一步提升。

=== Test 3: Reduce Hot Keys (HOT_KEYS=10) ===
LFU - 命中率: 92.10% (27630 / 30000)
// 热点更集中，LFU 效果更佳，命中率显著提升。

=== Test 4: High PUT rate (PUT=60%) ===
LFU - 命中率: 75.80% (22740 / 30000)
// 写操作增加，但 LFU 倾向于保留频率高的数据，对命中率影响相对 LRU 较小，仍能保持较高命中率。
```

**结论：** LFU 在热点访问场景下表现出色，能有效识别和保留高频数据。容量和热点集中度对命中率有积极影响。

### 循环扫描场景测试

```
=== Test 5: Loop Scan LFU ===
LFU - 命中率: 10.20% (3060 / 30000)
// 循环访问模式下，所有数据频率可能趋于一致，LFU 表现不佳，但可能略高于 LRU （取决于具体实现和驱逐辅助策略）。
```

**结论：** LFU 在纯循环扫描场景下表现不佳，因为缺乏区分度。然而，如果存在一些偶尔重复访问的数据，LFU 仍可能捕获到它们。

### LFU-Aging 测试场景分析

**目的：** 验证 LFU-Aging 机制在不同参数配置下的表现，特别是低 aging 阈值对缓存适应性的改进效果。

#### 4. LFU-Aging 基本功能测试

| 测试   | 参数            | 说明           | 预期结果             |
|--------|-----------------|----------------|----------------------|
| Test 1 | maxAvg=100     | 低阈值基线测试  | 频繁触发 aging，适应性增强 |
| Test 2 | maxAvg=50      | 极低阈值测试    | 更频繁的 aging，更快适应工作负载变化 |
| Test 3 | CAP=40, maxAvg=100 | 增大容量测试 | 在大容量下验证 aging 效果 |
| Test 4 | PUT=60%, maxAvg=100 | 高写入比例测试 | 写密集型场景下的 aging 表现 |
| Test 5 | HOT=10, maxAvg=100 | 热点集中测试 | 集中热点下的 aging 适应性 |

### 测试结果分析

#### LFU-Aging 测试结果

```
=== LFU-Aging Test 1: Low threshold (maxAvg=100) ===
GETs: X, Hits: Y, Hit Rate: Z%
// 基线测试：aging 机制开始生效，相比标准 LFU 在工作负载变化时适应性更强

=== LFU-Aging Test 2: Very low threshold (maxAvg=50) ===
GETs: X, Hits: Y, Hit Rate: Z%
// 极低阈值：更频繁的频率衰减，新热点数据更容易替换旧热点

=== LFU-Aging Test 3: Increase Capacity (CAPACITY=40) ===
GETs: X, Hits: Y, Hit Rate: Z%
// 大容量下的 aging：容量增加缓解了淘汰压力，aging 效果可能不如小容量明显

=== LFU-Aging Test 4: High PUT rate (PUT=60%) ===
GETs: X, Hits: Y, Hit Rate: Z%
// 高写入场景：频繁的 put 操作触发更多 aging，有助于快速适应新数据模式

=== LFU-Aging Test 5: Reduce Hot Keys (HOT_KEYS=10) ===
GETs: X, Hits: Y, Hit Rate: Z%
// 热点集中：在热点数据相对稳定时，aging 可能略微降低命中率，但增强了适应性
```

**结论：** 
- LFU-Aging 通过频率衰减机制有效解决了传统 LFU 的"缓存污染"问题
- 较低的 aging 阈值 (maxAvg=50-100) 能够提供更好的工作负载适应性
- 在写密集型场景下，aging 机制表现更为突出
- 需要在命中率和适应性之间找到平衡点

## 策略对比分析

### LFU
**优点：**
- 能够有效识别并长期保留真正的热点数据。
- 在访问频率稳定的场景下，通常能提供比 LRU 更高的命中率。

**缺点：**
- 对工作负载变化不敏感，旧热点数据可能长时间驻留缓存，即使它们已经不再被访问（"缓存污染"）。
- 实现相对复杂，需要维护每个 Key 的频率，并且在驱逐时需要高效地找到频率最低的数据。
- 冷启动问题：新加入缓存但访问频率还未累积起来的数据，在初期容易被淘汰。

### LFU-Aging

**优点：**
- 保持了 LFU 识别热点数据的优势，同时解决了"缓存污染"问题。
- 通过频率衰减机制，能够更快适应工作负载的变化。
- 在访问模式发生转移时，新热点数据能够更容易地替换旧热点数据。
- 可通过调节 aging 阈值来平衡稳定性和适应性。

**缺点：**
- 实现复杂度进一步增加，需要额外的逻辑来管理频率衰减。
- 需要合理配置 aging 阈值，过低可能导致缓存不稳定，过高可能失去 aging 效果。
- 频率衰减操作本身有一定的计算开销。
- 在访问模式相对稳定的场景下，可能会略微降低命中率（相对于标准 LFU）。

## 编译方式

使用 CMake 进行编译：

### ▶️ On Ubuntu (via WSL or native) or macOS:

```bash
cmake -B build
cmake --build build
./build/test_LfuCache
```

### ▶️ On Windows (via WSL):

```powershell
wsl bash -c "cmake -B build && cmake --build build && ./build/test_LfuCache"
```