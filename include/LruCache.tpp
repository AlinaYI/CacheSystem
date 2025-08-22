// ================================================================
//  LruCache.cpp  ——  LRU / LRU‑K / Hash‑LRU 的实现
// ================================================================

#include "LruCache.h"
#include <stdexcept>     // std::runtime_error
#include <thread>        // 获取硬件并发数（HashLRU 用）

namespace Cache {

// =============== LruCache 具体实现 =============== //

/**
 * ctor: 仅保存容量并创建 dummyHead / dummyTail
 */
template<typename K, typename V>
LruCache<K,V>::LruCache(int capacity) : capacity_(capacity)
{
    if (capacity_ <= 0)
        throw std::invalid_argument("capacity must be > 0");
    initializeList();
}

// -- public: put --------------------------------------------------
// 写入 / 更新：O(1)
// ---------------------------------------------------------------
template<typename K, typename V>
void LruCache<K,V>::put(const K& key, const V& value)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = nodeMap_.find(key);
    if (it != nodeMap_.end()) {
        updateExistingNode(it->second, value);
        return;
    }
    addNewNode(key, value);
}

// -- public: get  ----------------------------------------
// 若命中返回 true，并通过引用返回 value；否则 false
// ---------------------------------------------------------------
template<typename K, typename V>
bool LruCache<K,V>::get(const K& key, V& value)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = nodeMap_.find(key);
    if (it == nodeMap_.end()) return false;
    moveToMostRecent(it->second);
    value = it->second->value_;
    return true;
}

// -- public: get  ----------------------------------------
// 若未命中，抛出异常
// ---------------------------------------------------------------
template<typename K, typename V>
V LruCache<K,V>::get(const K& key)
{
    V tmp{};
    if (!get(key, tmp))
        throw std::runtime_error("Key not found in LRU cache");
    return tmp;
}

// -- public: remove ----------------------------------------------
template<typename K, typename V>
void LruCache<K,V>::remove(const K& key)
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
void LruCache<K,V>::initializeList()
{
    dummyHead_ = std::make_shared<Node>(K{}, V{});
    dummyTail_ = std::make_shared<Node>(K{}, V{});
    dummyHead_->next_ = dummyTail_;
    dummyTail_->prev_ = dummyHead_;
}

template<typename K, typename V>
void LruCache<K,V>::updateExistingNode(NodePtr node, const V& value)
{
    node->setValue(value);
    moveToMostRecent(node);
}

template<typename K, typename V>
void LruCache<K,V>::addNewNode(const K& key, const V& value)
{
    if (static_cast<int>(nodeMap_.size()) >= capacity_)
        evictLeastRecent();

    NodePtr n = std::make_shared<Node>(key, value);
    insertNode(n);
    nodeMap_[key] = n;
}

/** 把 node 移到链表尾（dummyTail_ 前）*/
template<typename K, typename V>
void LruCache<K,V>::moveToMostRecent(NodePtr node)
{
    removeNode(node);
    insertNode(node);
}

/** 断开 node 与链表的链接 */
template<typename K, typename V>
void LruCache<K,V>::removeNode(NodePtr node)
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
void LruCache<K,V>::insertNode(NodePtr node)
{
    node->next_ = dummyTail_;
    node->prev_ = dummyTail_->prev_;
    dummyTail_->prev_.lock()->next_ = node;
    dummyTail_->prev_ = node;
}

/** 删除链表头部真实节点（最久未使用） */
template<typename K, typename V>
void LruCache<K,V>::evictLeastRecent()
{
    NodePtr lru = dummyHead_->next_;
    if (lru == dummyTail_) return; // 不该发生
    removeNode(lru);
    nodeMap_.erase(lru->key_);
}

// ========= LruKCache =============================

template<typename K, typename V>
LruKCache<K,V>::LruKCache(int capacity, int keyRange, int k)
    : LruCache<K,V>(capacity), k_(k)
{
    (void)keyRange;
    historyList_ = std::make_unique<LruCache<K, size_t>>(capacity);
}

template<typename K, typename V>
bool LruKCache<K,V>::shouldPromote(const K& key, V& promotedValue) {
    (void)promotedValue;
    size_t historyCount = 0;
    historyList_->get(key, historyCount);
    historyCount++;
    historyList_->put(key, historyCount);

    if(historyCount >= static_cast<size_t>(k_)){
        // 这里用find更安全，map[key]会有默认值
        auto it = historyValueMap_.find(key);
        if (it != historyValueMap_.end()){
            V storedValue = it -> second;
            historyList_->remove(key);
            historyValueMap_.erase(it);
            return true;
        }
    }
    return false;
}

template<typename K, typename V>
void LruKCache<K, V>::put(const K& key, const V& value)
{
    V existingValue{};
    if (LruCache<K, V>::get(key, existingValue)) {
        LruCache<K, V>::put(key, value);
        return;
    }

    historyValueMap_[key] = value;

    V promoteValue;
    if (shouldPromote(key, promoteValue)) {
        LruCache<K, V>::put(key, promoteValue);
    }
}

template<typename K, typename V>
V LruKCache<K, V>::get(const K& key)
{
    V value;
    if (LruCache<K, V>::get(key, value)) return value;

    if (shouldPromote(key, value)) {
        LruCache<K, V>::put(key, value);
        return value;
    }

    return V{};
}

// ========= HashLruCaches(分片) =================================

// 构造函数
template<typename K, typename V>
HashLruCaches<K,V>::HashLruCaches(size_t cap, int slice)
    : capacity_(cap)
{
    sliceNum_ = slice > 0 ? slice : std::thread::hardware_concurrency(); // 默认按 CPU 核心数
    size_t sliceCap = std::ceil(capacity_ / static_cast<double>(sliceNum_)); // 每个 slice 的容量

    for (int i = 0; i < sliceNum_; ++i) {
        lruSlices_.emplace_back(std::make_unique<LruCache<K, V>>(sliceCap));
    }
}

// 哈希分片
template<typename K, typename V>
size_t HashLruCaches<K,V>::calcSliceIndex(const K& key) const {
    return std::hash<K>{}(key) % sliceNum_;
}

// put/get 调用目标分片
template<typename K, typename V>
void HashLruCaches<K,V>::put(const K& key, const V& value) {
    lruSlices_[calcSliceIndex(key)]->put(key, value);
}

template<typename K, typename V>
bool HashLruCaches<K,V>::get(const K& key, V& value) {
    return lruSlices_[calcSliceIndex(key)]->get(key, value);
}

template<typename K, typename V>
V HashLruCaches<K,V>::get(const K& key) {
    return lruSlices_[calcSliceIndex(key)]->get(key);
}


template class LruCache<int, std::string>;
template class LruKCache<int, std::string>;
template class HashLruCaches<int, std::string>;

} // namespace Cache