#pragma once

// =========================================================
//  LRU cache (interface declarations + richly commented)
//  ---------------------------------------------------------
//  The header only declares classes and member function prototypes.
//  Concrete implementations should live in src/LruCache.cpp.
// =========================================================

#include <list>            // Doubly linked list (can be swapped for another list impl later if needed)
#include <memory>          // Smart pointers: shared_ptr / weak_ptr
#include <mutex>           // std::mutex for thread safety
#include <unordered_map>   // Hash table: O(1) lookup to find the node for a key
#include <vector>          // Used by sharded (Hash) LRU
#include <cmath>           // std::ceil used in hash sharding

#include "CachePolicy.h"   // Common cache policy interface (defines put / get)

namespace Cache {

// =========================================================
// 1. Forward declaration: let LruNode and LruCache be friends
// =========================================================

template<typename Key, typename Value>
class LruCache;

// =========================================================
// 2. LruNode: list node holding a key/value pair + list links
// =========================================================

template<typename Key, typename Value>
class LruNode {
private:
    Key   key_;              // Cache key
    Value value_;            // Cache value
    size_t accessCount_{};   // Access counter, available for extensions

    // **Pointer notes**
    // next_ : shared_ptr  → owns the successor node
    // prev_ : weak_ptr    → observes the predecessor (no refcount increase; avoids cycles)
    std::weak_ptr<LruNode<Key, Value>> prev_;
    std::shared_ptr<LruNode<Key, Value>> next_;

public:
    // ----- Constructors -----
    LruNode(const Key& key, const Value& value)
        : key_(key), value_(value), accessCount_(1) {}

    // ----- Basic getters / setters -----
    const Key&   getKey()   const { return key_; }
    const Value& getValue() const { return value_; }
    void setValue(const Value& value) { value_ = value; }

    size_t getAccessCount() const { return accessCount_; }
    void   incrementAccessCount() { ++accessCount_; }

    // Allow LruCache to access private members
    friend class LruCache<Key, Value>;
};

// =========================================================
// 3. LruCache: standard least-recently-used cache (declaration)
// =========================================================

template<typename Key, typename Value>
class LruCache : public CachePolicy<Key, Value> {
public:
    using Node      = LruNode<Key, Value>;
    using NodePtr   = std::shared_ptr<Node>;
    using NodeMap   = std::unordered_map<Key, NodePtr>;

    explicit LruCache(int capacity);
    ~LruCache() override = default;

    // ---- Interface functions (must be implemented, see .cpp) ----
    void   put(const Key& key, const Value& value) override;   // Write / update
    bool   get(const Key& key, Value& value) override;         // Read (safe version)
    Value  get(const Key& key) override;                       // Read (convenience version)
    void   remove(const Key& key);                             // Erase a key

private:
    // ---- Internal helpers ----
    void initializeList();                                     // Create dummyHead / dummyTail
    void updateExistingNode(NodePtr node, const Value& value); // Update on hit
    void addNewNode(const Key& key, const Value& value);       // Add when not present
    void moveToMostRecent(NodePtr node);                       // Move to list tail
    void removeNode(NodePtr node);                             // Remove from list
    void insertNode(NodePtr node);                             // Insert at list tail
    void evictLeastRecent();                                   // Evict when over capacity

private:
    int       capacity_{};   // Cache capacity
    NodeMap   nodeMap_;      // key → NodePtr
    std::mutex mutex_;       // Global lock (simple thread-safety approach)
    NodePtr   dummyHead_;    // Sentinel head node
    NodePtr   dummyTail_;    // Sentinel tail node
};

// =========================================================
// 4. LruKCache: LRU-K improved cache (interface only)
// =========================================================

template<typename Key, typename Value>
class LruKCache : public LruCache<Key, Value> {
public:
    LruKCache(int capacity, int historyCapacity, int k);

    // Override put / get to implement the “admit after K hits” logic
    void  put(const Key& key, const Value& value);
    Value get(const Key& key);

private:
    bool shouldPromote(const Key&, Value&);

private:
    int                                      k_;               // Hit threshold to enter the main cache
    std::unique_ptr<LruCache<Key, size_t>>   historyList_;     // Tracks per-key access counts
    std::unordered_map<Key, Value>           historyValueMap_; // Holds values until hits reach k
};

// =========================================================
// 5. HashLruCaches: sharded LRU to improve concurrency (declaration)
// =========================================================

template<typename Key, typename Value>
class HashLruCaches {
public:
    HashLruCaches(size_t capacity, int sliceNum = 0);          // sliceNum=0 → default to CPU core count

    void  put(const Key& key, const Value& value);
    bool  get(const Key& key, Value& value);
    Value get(const Key& key);

private:
    size_t calcSliceIndex(const Key& key) const;                // Compute which shard a key belongs to

private:
    size_t capacity_;       // Total capacity
    int    sliceNum_;       // Number of shards
    std::vector<std::unique_ptr<LruCache<Key, Value>>> lruSlices_; // Multiple sub-caches
};

} // namespace Cache

#include "../src/LruCache.tpp"