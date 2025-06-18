#pragma once

// =========================================================
//  LRU 缓存（接口声明 + 详细注释版）
//  ---------------------------------------------------------
//  该头文件只声明类和成员函数原型，不包含实现代码；
//  具体实现应放在 src/KLruCache.cpp 中。
// =========================================================

#include <list>            // 双向链表（如果以后要改 list 实现，可保留）
#include <memory>          // 智能指针 shared_ptr / weak_ptr
#include <mutex>           // std::mutex 上锁保证线程安全
#include <unordered_map>   // 哈希表：O(1) 查找 key 所在的节点
#include <vector>          // 分片 LRU 需要用到
#include <cmath>           // std::ceil 在 Hash 分片中用

#include "CachePolicy.h" // 通用缓存策略接口（定义 put / get）

namespace Cache {

// =========================================================
// 1. 前向声明：让 LruNode 和 KLruCache 可以互相作为 friend
// =========================================================

template<typename Key, typename Value>
class KLruCache; // 注意：拼写必须和后面类名一致

// =========================================================
// 2. LruNode：链表节点，持有一个键值对 + 链表指针
// =========================================================

template<typename Key, typename Value>
class LruNode {
private:
    Key   key_;              // 缓存 key
    Value value_;            // 缓存 value
    size_t accessCount_{};   // 访问计数，可供扩展

    // **指针说明**
    // next_  : shared_ptr  → 拥有后继节点的所有权
    // prev_  : weak_ptr    → 观察前驱节点（不增加引用计数，避免循环引用）
    std::weak_ptr<LruNode<Key, Value>> prev_;
    std::shared_ptr<LruNode<Key, Value>> next_;

public:
    // ----- 构造函数 -----
    LruNode(const Key& key, const Value& value)
        : key_(key), value_(value), accessCount_(1) {}

    // ----- 基本 Getter / Setter -----
    const Key&   getKey()   const { return key_; }
    const Value& getValue() const { return value_; }
    void setValue(const Value& value) { value_ = value; }

    size_t getAccessCount() const { return accessCount_; }
    void   incrementAccessCount() { ++accessCount_; }

    // 让 KLruCache 能访问本类私有成员
    friend class KLruCache<Key, Value>;
};

// =========================================================
// 3. KLruCache：标准最近最少使用缓存（声明）
// =========================================================

template<typename Key, typename Value>
class KLruCache : public CachePolicy<Key, Value> {
public:
    using Node      = LruNode<Key, Value>;
    using NodePtr   = std::shared_ptr<Node>;
    using NodeMap   = std::unordered_map<Key, NodePtr>;

    explicit KLruCache(int capacity);
    ~KLruCache() override = default;

    // ---- 接口函数（必须实现，见 .cpp）----
    void   put(const Key& key, const Value& value) override;          // 写入 / 更新
    bool   get(const Key& key, Value& value) override;               // 读取（安全版）
    Value  get(const Key& key) override;                             // 读取（便捷版）
    void   remove(const Key& key);                                   // 删除 key

private:
    // ---- 内部辅助函数 ----
    void initializeList();                                           // 创建 dummyHead / dummyTail
    void updateExistingNode(NodePtr node, const Value& value);       // 命中更新
    void addNewNode(const Key& key, const Value& value);             // 不存在时添加
    void moveToMostRecent(NodePtr node);                             // 移到链表尾
    void removeNode(NodePtr node);                                   // 从链表中移除
    void insertNode(NodePtr node);                                   // 插入链表尾
    void evictLeastRecent();                                         // 超容量驱逐

private:
    int       capacity_{};   // 缓存容量
    NodeMap   nodeMap_;      // key → NodePtr
    std::mutex mutex_;       // 全局锁（简单线程安全实现）
    NodePtr   dummyHead_;    // 虚拟头节点
    NodePtr   dummyTail_;    // 虚拟尾节点
};

// =========================================================
// 4. KLruKCache：LRU-K 改良版缓存（只声明接口）
// =========================================================

template<typename Key, typename Value>
class KLruKCache : public KLruCache<Key, Value> {
public:
    KLruKCache(int capacity, int historyCapacity, int k);

    // 重写 put / get，实现“K 次命中才进入主缓存”逻辑
    void  put(const Key& key, const Value& value);
    Value get(const Key& key);

private:
    int                                      k_;              // 进入主缓存所需的命中阈值
    std::unique_ptr<KLruCache<Key, size_t>>  historyList_;    // 记录每个 key 的访问次数
    std::unordered_map<Key, Value>           historyValueMap_; // 保存 value，直到命中次数达到 k
};

// =========================================================
// 5. KHashLruCaches：分片 LRU，提高并发性能（声明）
// =========================================================

template<typename Key, typename Value>
class KHashLruCaches {
public:
    KHashLruCaches(size_t capacity, int sliceNum = 0);          // sliceNum=0 → 默认按 CPU 核心数

    void  put(const Key& key, const Value& value);
    bool  get(const Key& key, Value& value);
    Value get(const Key& key);

private:
    size_t calcSliceIndex(const Key& key) const;                 // 计算 key 应进入哪个分片

private:
    size_t                                              capacity_;       // 总容量
    int                                                 sliceNum_;       // 分片数
    std::vector<std::unique_ptr<KLruCache<Key, Value>>> lruSlices_;      // 多个子缓存
};

} // namespace Cache
