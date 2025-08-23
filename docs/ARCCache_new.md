# NewArc (ARC-inspired, Engineering-oriented) 缓存策略

## NewARC 是一种结合 LRU 和 LFU 的工程化缓存算法

NewARC 是一种受 ARC 启发的缓存策略，它通过 LRU（最近最少使用） 和 LFU（最不经常使用） 的分区管理，结合幽灵缓存和阈值晋升机制，在不同访问模式下保持较高的命中率。与标准 ARC 相比，KArc 更符合工程应用场景，逻辑直观且能精确捕捉长期热点。

### 适用场景
- 同时存在短期热点和长期热点的数据访问场景。
- 需要避免单纯 LRU 的“扫描污染”，同时克服 LFU 的“历史包袱”。
- 工程系统中（数据库、分布式存储、KV 缓存）需要可解释、可调节的策略。

### 设计思路
NewARC 算法通过维护 LRU 部分、LFU 部分 和各自的 幽灵列表 来实现缓存管理：

-   **LRU 部分**：存放*新近访问*的数据项。节点带有访问计数 accessCount。
-   **LRU Ghost**：记录最近从 LRU 中淘汰的键。
-   **LFU 部分**：存放*长期访问频繁*的数据，使用 map<频率, list<节点>> 精确管理。
-   **LFU Ghost**：记录最近从 LFU 中淘汰的键。

**容量自适应机制:**
如果某个键在 LRU Ghost 中命中 → 表示近期性更重要 → 给 LRU +1 容量，从 LFU -1。
如果某个键在 LFU Ghost 中命中 → 表示频率性更重要 → 给 LFU +1 容量，从 LRU -1。

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

### NewARC 接口和实现

**头文件设计：**
- `ArcLruPart`：实现 LRU 部分，支持节点晋升、维护 LRU Ghost。
- `ArcLfuPart`：实现 LFU 部分，支持频率桶、维护 LFU Ghost。
- `NewArcCache`：顶层类，组合 LRU 与 LFU 两部分，并处理自适应容量调整。

**实现文件特点：**
- `put(const Key& key, const Value& value)`：优先写入 LRU；如果节点晋升则进入 LFU；命中幽灵时触发容量调整。
- `get(const Key& key, Value& value)`：查找 LRU → 如果满足晋升条件则进入 LFU；否则查找 LFU。
- `checkGhostCaches(Key)`: 判断命中的幽灵缓存，并触发 LRU/LFU 容量的增减。

## 测试设计

### 测试目标
- 验证 NewArc 的基本功能：put、get、晋升逻辑、容量调整。
- 测试 NewArc 在不同访问模式下的命中率表现。

### 测试场景

#### 1. 热点访问场景 (Hot Access)
**目的：** 模拟大部分访问集中在少数 key 上，观察 NewArc 如何利用晋升机制将热点提升到 LFU。

| 测试   | 参数            | 说明           | 预期结果             |
|--------|-----------------|----------------|----------------------|
| Test 1 | CAP=20, HOT=20  | 基线测试        | 命中率高于 LRU 和 LFU |
| Test 2 | CAP=40          | 增大缓存容量    | 命中率显著提升，保持高位 |
| Test 3 | HOT=10          | 热点更集中      | 命中率进一步上升，稳定在高点 |

#### 2. 循环扫描场景 (Loop Scan)
**目的：** 模拟数据循环访问场景，测试幽灵缓存能否避免频繁误驱逐

| 测试   | 参数             | 说明     | 预期结果                 |
|--------|------------------|----------|--------------------------|
| Test 4 | CAP=20, LOOP=500 | 循环访问  | 命中率高于 LRU，但接近 ARC，整体仍低 |

#### 3. 访问频率变化场景 (Workload Shift Simulation)
**目的：** 模拟工作负载从一组热点数据转移到另一组热点数据的情况，验证 ARC 的自适应性。

| 测试   | 参数               | 说明                     | 预期结果                               |
|--------|--------------------|--------------------------|----------------------------------------|
| Test 5 | Shift Workload     | 热点数据转移             | NewARC 能够较快适应新热点，命中率短期波动后恢复 |

#### 4. 幽灵列表命中场景 (Ghost List Hits)
**目的：** 专门测试 NewARC 的容量调整逻辑。

| 测试   | 场景               | 预期结果                                |
|--------|--------------------|----------------------------------------|
| Test 6 | LRU Ghost          | LRU +1, LFU -1 → 提升短期命中率         |
| Test 7 | LFU Ghost          | LFU +1, LRU -1 → 提升长期热点稳定性     |

## 测试结果 (示例，实际值可能因实现细节和随机性而异)

### 热点访问场景测试


## 编译方式

使用 CMake 进行编译：

### ▶️ On Ubuntu (via WSL or native) or macOS:

```bash
cmake -B build
cmake --build build
./build/test_ArcCache
```

### ▶️ On Windows (via WSL):

```powershell
wsl bash -c "cd /home/alina/CacheSystem && cmake -B build && cmake --build build && ./build/test_ArcCache"
```

### ▶️ On macOS

```bash
cmake -B build
cmake --build build
./build/test_ArcCache
```
