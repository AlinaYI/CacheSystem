#pragma once

#include "CachePolicy.h"
#include <list>
#include <unordered_map>
#include <mutex>
#include <algorithm>

namespace Cache {

template <typename Key, typename Value>
class Arc_new : public CachePolicy<Key, Value> {
public:
    explicit Arc_new(size_t capacity)
        : capacity_(capacity), p_(0) {}

    ~Arc_new() override = default;

    // CachePolicy interface
    void   put(const Key& key, const Value& value) override;
    bool   get(const Key& key, Value& out) override;
    Value  get(const Key& key) override;

    // Utility methods
    void   clear();
    size_t size() const;              // T1 + T2
    size_t capacity() const { return capacity_; }
    size_t p() const { return p_; }
    bool   contains(const Key& key) const;

private:
    enum class ListTag { None, T1, T2 };

    struct Entry {
        Value value{};
        ListTag tag{ListTag::None};
        typename std::list<Key>::iterator it;  // Iterator pointing to the key's position in T1/T2
    };

    // Four lists: T1/T2 are real cache; B1/B2 are ghost lists (keys only)
    std::list<Key> t1_, t2_, b1_, b2_;

    // Real cache index (only T1/T2 hold values)
    std::unordered_map<Key, Entry> map_;

    // Ghost list indexes (for O(1) access to list iterators)
    std::unordered_map<Key, typename std::list<Key>::iterator> b1_map_, b2_map_;

    size_t capacity_{0}; // Real cache capacity (T1+T2)
    size_t p_{0};        // Target size of T1 (0..capacity_)

    mutable std::mutex mtx_;

private:
    // —— Core algorithm —— //
    void replace(bool hit_in_b1);  // Evict from T1 or T2 to B1/B2
    void adjustPOnB1Hit();         // On B1 hit: increase p (favor recency)
    void adjustPOnB2Hit();         // On B2 hit: decrease p (favor frequency)

    // —— List/index operations —— //
    void moveToT2(const Key& key);
    void addToT1MRU(const Key& key, const Value& val);
    void addToT2MRU(const Key& key, const Value& val);

    void evictFromT1ToB1();
    void evictFromT2ToB2();

    // Keep ghost lists bounded: |B1|, |B2| ≤ capacity_
    void trimGhost(std::list<Key>& blist,
                   std::unordered_map<Key, typename std::list<Key>::iterator>& bmap);

    // Helpers: push front to list / erase by iterator
    static typename std::list<Key>::iterator attachFront(std::list<Key>& lst, const Key& key);
    static void detach(std::list<Key>& lst, typename std::list<Key>::iterator it);
};

} // namespace Cache

// Template implementation
#include "../src/Arc_new.tpp"
