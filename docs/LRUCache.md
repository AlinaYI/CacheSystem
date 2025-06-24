# LRU (Least Recently Used) 缓存策略

## LRU 作为做常见的缓存策略，也就是淘汰掉最近最久未被访问的数据

LRU（Least Recently Used）是最常见的缓存策略，其核心思想是淘汰掉最近最久未被访问的数据。

### 适用场景
- 存在明显"热点数据"的情况
- 访问频率比较稳定，较少短期突变的场景

### 设计思路
- 使用双向链表（double linked list）维护容量和数据
- 把最新的数据放到最前面，如果容量超了，弹出最后面的节点
- 通过维护时间顺序，尽量提高get的命中率

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
    virtual Value get(const Key&) = 0;
    virtual ~CachePolicy() = default;
};
```

### LRU 接口和实现

**头文件设计：**
- 声明了 `LruNode` 节点类：双向链表结构，记录 Key/Value
- 声明了 `KLruCache` 节点类：继承接口，实现LRU逻辑

**实现文件特点：**
- `std::unordered_map<Key, NodePtr>` 实现 O(1) 访问
- 双向链表维护访问顺序，使用 dummyHead/dummyTail 作为头尾
- 使用 `std::weak_ptr` 和 `std::shared_ptr`：
  - `next_`：`shared_ptr` → 拥有后继节点的所有权
  - `prev_`：`weak_ptr` → 观察前驱节点（不增加引用数，避免循环引用）
- 为确保多线程安全性，在 put/get/remove 操作中加入 mutex，确保数据安全性和一致性

## 缓存策略变体

### 1. KLruCache

**核心思想：** 只有访问 ≥ K 次的数据才允许进入主缓存，防止冷数据污染缓存。

这是对LRU的改进，使用两个队列实现：
- 缓存队列：存储热点数据
- 数据访问历史队列：记录访问次数

当访问数据时，先在访问历史队列中累加访问次数，当历史访问记录超过k次后，才将数据缓存到缓存队列，避免缓存队列被污染。

**缓存污染：** 访问很少的数据在服务完访问请求后还留在缓存中，造成缓存空间浪费。

**注意事项：** k值越大，缓存命中率越高，但也会让缓存难以被淘汰。

### 2. HashLruCache（轻量级LRU）

**核心思想：** 把一个大缓存切成多个小片段，每个片段维护一个独立的LRU，这样多个线程访问不同key时不会互相锁住。

**适应场景：** 高并发场景

对于大量同步等待操作导致耗时增加的情况，解决方案是尽量减少临界区。引入hash从而降低查询耗时：
- 将缓存数据分散到N个LruCache上
- 查询时按照相同的哈希算法，先获取数据可能存在的分片，再去对应的分片上查询数据
- 增加LRU读写操作的并行度，减少同步等待的耗时

## 测试设计

### 测试目标
- 验证LRU的基本功能
- 测试命中率在不同访问模式下的表现
- 对比不同变量（capacity、访问模式）对结果的影响

### 测试场景

#### 1. 热点访问场景 (Hot Access)
**目的：** 模拟大部分访问集中在少数key上，比如热门网页或商品。

| 测试     | 参数              | 说明           | 预期结果          |
|----------|-------------------|----------------|-------------------|
| Test 1   | CAP=20, HOT=20    | 基线测试        | LRU 命中率中等    |
| Test 2   | CAP=40            | 增大缓存容量    | 命中率明显提升    |
| Test 3   | HOT=10            | 热点更集中      | 命中率进一步上升  |
| Test 4   | PUT=60%           | 写操作比例增加  | 可能会影响命中率  |

#### 2. LRU-K 对比 LRU
**目的：** 验证当热点访问频繁时，LRU-K是否更能识别并缓存热点数据。

| 测试     | 参数 | 说明             | 预期结果           |
|----------|------|------------------|-------------------|
| Test 5   | k=2  | LRU-K vs LRU     | LRU-K 命中率略高   |
| Test 6   | k=3  | 更严格的提升规则 | 可能因晋升门槛高反而效果不如LRU |

#### 3. 循环扫描场景 (Loop Scan)
**目的：** 模拟数据循环访问场景，常用于顺序读块的模式，观察缓存策略的应对能力。

| 测试     | 参数             | 说明     | 预期结果                |
|----------|------------------|----------|-------------------------|
| Test 7   | CAP=20, LOOP=500 | LRU      | 命中率非常低（缓存频繁失效） |
| Test 8   | k=2              | LRU-K    | 提升有限或无明显效果     |

#### 4. 冷数据场景 (Cold Data)
**目的：** 模拟每次访问的key都是新数据，对任何缓存策略都是极端不利场景。

| 测试      | 参数                | 说明     | 预期结果            |
|-----------|---------------------|----------|---------------------|
| Test 9    | CAP=20, RANGE=10000 | LRU      | 命中率几乎为0        |
| Test 10   | k=2                 | LRU-K    | 与LRU表现基本一致    |

#### 5. Hash-LRU 对比测试
**目的：** 验证Hash分片缓存策略在多线程或分布式访问下是否带来提升。

| 测试      | 参数       | 说明                        | 预期结果               |
|-----------|------------|----------------------------|----------------------|
| Test 11   | slice=1    | Hash-LRU 基线配置           | 与普通LRU相近          |
| Test 12   | slice=4    | 开启多分片                  | 命中率略有提升（访问并发解耦） |
| Test 13   | LOOP场景   | 分片在顺序扫描下不一定占优   | 可能与普通LRU相近      |
| Test 14   | Cold Data  | 分片策略对冷数据无明显优化效果 | 命中率仍非常低         |

## 测试结果

### 热点访问场景测试

```
=== Test 1: Baseline (CAPACITY=20, HOT_KEYS=20) ===
GETs: 70146, Hits: 34511, Hit Rate: 49.20%
// baseline 命中率中等，表示热点刚好覆盖满缓存，频繁写入与替换会让缓存不稳定

=== Test 2: Increase Capacity (CAPACITY=40) ===
GETs: 69905, Hits: 49012, Hit Rate: 70.11%
// 容量翻倍，有更多空间存热点key，命中率显著提升

=== Test 3: Reduce Hot Keys (HOT_KEYS=10) ===
GETs: 70218, Hits: 48889, Hit Rate: 69.62%
// 热点集中性更强，缓存能更快收敛热点key，命中率也接近Test 2

=== Test 4: High PUT rate (PUT=60%) ===
GETs: 39931, Hits: 18036, Hit Rate: 45.17%
// 写多导致缓存数据更换更频繁，命中率下降，符合预期
```

**结论：** 热点访问场景整体命中率变动与参数成正相关，说明LRU能比较敏感地响应热点key的集中度和容量变化。

### LRU-K 测试

```
=== Test 5: LRU-K vs LRU (k=2) ===
GETs: 70221, Hits: 48595, Hit Rate: 69.20%
// 命中率接近Test 2和3，略低于LRU，因为需要达到两次访问才晋升为缓存内容

=== Test 6: LRU-K (k=3) ===
GETs: 70264, Hits: 48957, Hit Rate: 69.68%
// 略高于Test 5，表示热key被稳定晋升，但也说明Test 5可能晋升略慢，Test 6参数反而合理
```

**结论：** LRU-K整体命中率与普通LRU相差不大，在热点足够集中的情况下效果不错，尤其当热点key能快速晋升。Test 6表现比想象中好，说明热点key访问非常频繁。

### 循环扫描场景测试

```
=== Test 7: Loop Scan LRU ===
GETs: 79992, Hits: 0, Hit Rate: 0.00%
// 每次访问key都被替换，基本没有复用空间

=== Test 8: Loop Scan LRU-K (k=2) ===
GETs: 79971, Hits: 0, Hit Rate: 0.00%
// 无法达到晋升次数，必然全miss
```

**结论：** 完美符合预期，循环访问和FIFO特性导致缓存彻底失效。这类访问模式就是LRU系策略的弱点。

### 冷数据场景测试

```
=== Test 9: All Cold Data LRU ===
GETs: 50000, Hits: 50000, Hit Rate: 100.00%
// 正常应该全miss，但在put后立刻get，所以每个值都被命中。可以算成"写后立刻读"而非真实cold data

=== Test 10: All Cold Data LRU-K (k=2) ===
GETs: 50000, Hits: 232, Hit Rate: 0.46%
// 由于不能达到K次访问，基本都miss，符合cold data的效果
```

**结论：** Test 9的"命中率100%"是因为测试逻辑不是cold scan，而是write-followed-by-read。可以接受，但文档里要注明："此为put后立即get场景，不是纯cold miss"。

### Hash-LRU 测试

```
=== Test 11: Hash LRU (default slice) ===
GETs: 70208, Hits: 43176, Hit Rate: 61.50%
// 表现优于baseline LRU（Test 1），但略逊于LRU-K（Test 6）

=== Test 12: Hash LRU (4 slices) ===
GETs: 70212, Hits: 34233, Hit Rate: 48.76%
// 多分片本应提升并发访问能力，但命中率下降，可能key分布不均或每片容量太小

=== Test 13: Loop Scan Hash LRU ===
GETs: 80223, Hits: 150, Hit Rate: 0.19%
// 微弱提升，分片略微减缓了key被踢出，但仍然是坏场景

=== Test 14: All Cold Data Hash LRU ===
GETs: 50000, Hits: 50000, Hit Rate: 100.00%
// 说明put后立即get，导致命中率人为偏高
```

**结论：** Hash LRU的slice设置要根据容量配合，不然小slice会导致局部命中率下降。Test 12的表现说明多分片不等于一定更优，具体还需合理设置key hash分布和slice容量。Cold Data测试中put后立即get的逻辑要特别注意，和实际场景偏差大。

## 策略对比分析

### LRU
**优点：**
- 实现简单
- 命中率稳定

**缺点：**
- 易受冷数据、顺序扫描影响
- 缺乏"过滤能力"

### LRU-K
**优点：**
- 能有效过滤掉一次性访问
- 提高热点命中率

**缺点：**
- 初始化阶段"冷启动"
- 实现相对复杂
- 不适合对延迟敏感的业务

### Hash-LRU
**优点：**
- 天然适用于多线程/多租户高并发环境
- 防止集中热点冲突

**缺点：**
- 命中率依赖key分布
- 分片过多会导致单组容量太小、效率下降

## 编译方式

使用 CMake 进行编译：

```bash
cmake -B build
cmake --build build
./build/test_lru_only
```
