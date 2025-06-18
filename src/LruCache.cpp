// ================================================================
//  KLruCache.cpp  ——  LRU / LRU‑K / Hash‑LRU 的示例实现模板
//  --------------------------------------------------------------
//  说明：
//  1. 这是与 include/KLruCache.h 配套的实现文件；
//     每个函数都给出『最小可运行』逻辑 + 大段 TODO 注释，
//     你可以在 TODO 区按提示继续完善或做性能优化。
//  2. 为了让你一粘贴就能编译，本文件已包含头文件并
//     明确在最底部做了模板实例化（典型做法）。
//  3. 如果你想拆分成多个 .cpp，也没问题——只要保持
//     模板实例化在某个编译单元即可。
// ================================================================

#include "KLruCache.h"  // 记得路径根据你的 include 目录调整
#include <stdexcept>     // std::runtime_error
#include <thread>        // 获取硬件并发数（HashLRU 用）

namespace Cache {

// =============== KLruCache 具体实现 =============== //

/**
 * ctor: 仅保存容量并创建 dummyHead / dummyTail
 */
template<typename K, typename V>
KLruCache<K,V>::KLruCache(int capacity) : capacity_(capacity)
{
    if (capacity_ <= 0)
        throw std::invalid_argument("capacity must be > 0");
    initializeList();
}

// -- public: put --------------------------------------------------
// 写入 / 更新：O(1)
// ---------------------------------------------------------------
template<typename K, typename V>
void KLruCache<K,V>::put(const K& key, const V& value)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = nodeMap_.find(key);
    if (it != nodeMap_.end()) {
        updateExistingNode(it->second, value);
        return;
    }
    addNewNode(key, value);
}

// -- public: get (安全版) ----------------------------------------
// 若命中返回 true，并通过引用返回 value；否则 false
// ---------------------------------------------------------------
template<typename K, typename V>
bool KLruCache<K,V>::get(const K& key, V& value)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = nodeMap_.find(key);
    if (it == nodeMap_.end()) return false;
    moveToMostRecent(it->second);
    value = it->second->value_;
    return true;
}

// -- public: get (便捷版) ----------------------------------------
// 若未命中，抛出异常；你可改成返回 optional
// ---------------------------------------------------------------
template<typename K, typename V>
V KLruCache<K,V>::get(const K& key)
{
    V tmp{};
    if (!get(key, tmp))
        throw std::runtime_error("Key not found in LRU cache");
    return tmp;
}

// -- public: remove ----------------------------------------------
template<typename K, typename V>
void KLruCache<K,V>::remove(const K& key)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = nodeMap_.find(key);
    if (it == nodeMap_.end()) return;
    removeNode(it->second);
    nodeMap_.erase(it);
}

// -- private helpers ---------------------------------------------

/** 创建 dummyHead / dummyTail 并互链 */
template<typename K, typename V>
void KLruCache<K,V>::initializeList()
{
    dummyHead_ = std::make_shared<Node>(K{}, V{});
    dummyTail_ = std::make_shared<Node>(K{}, V{});
    dummyHead_->next_ = dummyTail_;
    dummyTail_->prev_ = dummyHead_;
}

template<typename K, typename V>
void KLruCache<K,V>::updateExistingNode(NodePtr node, const V& value)
{
    node->setValue(value);
    moveToMostRecent(node);
}

template<typename K, typename V>
void KLruCache<K,V>::addNewNode(const K& key, const V& value)
{
    if (static_cast<int>(nodeMap_.size()) >= capacity_)
        evictLeastRecent();

    NodePtr n = std::make_shared<Node>(key, value);
    insertNode(n);
    nodeMap_[key] = n;
}

/** 把 node 移到链表尾（dummyTail_ 前）*/
template<typename K, typename V>
void KLruCache<K,V>::moveToMostRecent(NodePtr node)
{
    removeNode(node);
    insertNode(node);
}

/** 断开 node 与链表的链接 */
template<typename K, typename V>
void KLruCache<K,V>::removeNode(NodePtr node)
{
    auto prev = node->prev_.lock();
    auto next = node->next_;
    if (prev && next) {
        prev->next_ = next;
        next->prev_ = prev;
    }
}

/** 把 node 插入到尾部 */
template<typename K, typename V>
void KLruCache<K,V>::insertNode(NodePtr node)
{
    node->next_ = dummyTail_;
    node->prev_ = dummyTail_->prev_;
    dummyTail_->prev_.lock()->next_ = node;
    dummyTail_->prev_ = node;
}

/** 删除链表头部真实节点（最久未使用） */
template<typename K, typename V>
void KLruCache<K,V>::evictLeastRecent()
{
    NodePtr lru = dummyHead_->next_;
    if (lru == dummyTail_) return; // 不该发生
    removeNode(lru);
    nodeMap_.erase(lru->key_);
}

// ========= KLruKCache(示例简版实现) =============================
// TODO：你可以逐步完善访问次数逻辑，这里给最简路径保持可编译
// ================================================================

template<typename K, typename V>
KLruKCache<K,V>::KLruKCache(int cap,int histCap,int k)
    : KLruCache<K,V>(cap), k_(k)
{
    historyList_ = std::make_unique<KLruCache<K,size_t>>(histCap);
}

template<typename K, typename V>
void KLruKCache<K,V>::put(const K& key, const V& value)
{
    KLruCache<K,V>::put(key,value); // 先直接复用父类逻辑
    // TODO: 记录历史计数并根据 k_ 决定是否真正写入
}

template<typename K, typename V>
V KLruKCache<K,V>::get(const K& key)
{
    return KLruCache<K,V>::get(key); // 简化实现
}

// ========= KHashLruCaches(分片) =================================

template<typename K, typename V>
KHashLruCaches<K,V>::KHashLruCaches(size_t cap,int slice)
    : capacity_(cap)
{
    sliceNum_ = slice > 0 ? slice : std::thread::hardware_concurrency();
    size_t sliceCap = std::ceil(capacity_ / static_cast<double>(sliceNum_));
    for(int i=0;i<sliceNum_;++i)
        lruSlices_.emplace_back(std::make_unique<KLruCache<K,V>>(sliceCap));
}

template<typename K, typename V>
size_t KHashLruCaches<K,V>::calcSliceIndex(const K& key) const
{
    return std::hash<K>{}(key) % sliceNum_;
}

template<typename K, typename V>
void KHashLruCaches<K,V>::put(const K& key,const V& value)
{
    lruSlices_[calcSliceIndex(key)]->put(key,value);
}

template<typename K, typename V>
bool KHashLruCaches<K,V>::get(const K& key,V& value)
{
    return lruSlices_[calcSliceIndex(key)]->get(key,value);
}

template<typename K, typename V>
V KHashLruCaches<K,V>::get(const K& key)
{
    return lruSlices_[calcSliceIndex(key)]->get(key);
}

// ================================================================
// 显式模板实例化：把常用 <int,std::string> 组合提前编译，
// 避免每个 .cpp 重复生成代码（可根据需要扩充）
// ================================================================

template class KLruCache<int,std::string>;
template class KLruKCache<int,std::string>;
template class KHashLruCaches<int,std::string>;

} // namespace Cache
