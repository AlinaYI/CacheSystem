#pragma once
#include <cassert>

namespace Cache {

// ===== Construction / Basics =====
template <typename Key, typename Value>
void Arc_new<Key, Value>::clear() {
    std::lock_guard<std::mutex> lk(mtx_);
    t1_.clear(); t2_.clear(); b1_.clear(); b2_.clear();
    map_.clear(); b1_map_.clear(); b2_map_.clear();
    p_ = 0;
}

template <typename Key, typename Value>
size_t Arc_new<Key, Value>::size() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return t1_.size() + t2_.size();
}

template <typename Key, typename Value>
bool Arc_new<Key, Value>::contains(const Key& key) const {
    std::lock_guard<std::mutex> lk(mtx_);
    return map_.find(key) != map_.end();
}

// ===== CachePolicy interface: get / put =====
template <typename Key, typename Value>
bool Arc_new<Key, Value>::get(const Key& key, Value& out) {
    std::lock_guard<std::mutex> lk(mtx_);

    // Hit in T1/T2: move to T2's MRU
    if (auto it = map_.find(key); it != map_.end()) {
        out = it->second.value;
        moveToT2(key);
        return true;
    }

    // Hit in B1/B2: standard ARC ghosts hold no values → only used for tuning and replacement
    if (auto b1it = b1_map_.find(key); b1it != b1_map_.end()) {
        // Remove the ghost first (avoid iterator invalidation during replace -> trimGhost)
        auto nodeIt = b1it->second;
        detach(b1_, nodeIt);
        b1_map_.erase(b1it);

        adjustPOnB1Hit();
        replace(true /*hit_in_b1*/);
        return false; // Requires upper layer to load then put
    }
    if (auto b2it = b2_map_.find(key); b2it != b2_map_.end()) {
        auto nodeIt = b2it->second;
        detach(b2_, nodeIt);
        b2_map_.erase(b2it);

        adjustPOnB2Hit();
        replace(false /*hit_in_b1*/);
        return false; // Same as above
    }

    // Completely missed
    return false;
}

template <typename Key, typename Value>
Value Arc_new<Key, Value>::get(const Key& key) {
    Value v{};
    get(key, v);
    return v;
}

template <typename Key, typename Value>
void Arc_new<Key, Value>::put(const Key& key, const Value& value) {
    std::lock_guard<std::mutex> lk(mtx_);

    // Already in T1/T2: update and move to T2
    if (auto it = map_.find(key); it != map_.end()) {
        it->second.value = value;
        moveToT2(key);
        return;
    }

    // ——— Ghost hit: remove ghost first → adjust p → replace → insert into T2 ———
    if (auto b1it = b1_map_.find(key); b1it != b1_map_.end()) {
        auto nodeIt = b1it->second;
        detach(b1_, nodeIt);
        b1_map_.erase(b1it);

        adjustPOnB1Hit();
        replace(true);
        addToT2MRU(key, value);
        return;
    }
    if (auto b2it = b2_map_.find(key); b2it != b2_map_.end()) {
        auto nodeIt = b2it->second;
        detach(b2_, nodeIt);
        b2_map_.erase(b2it);

        adjustPOnB2Hit();
        replace(false);
        addToT2MRU(key, value);
        return;
    }

    // ——— Brand-new key: replace if necessary, then insert into T1 ———
    if (capacity_ == 0) {
        // No real cache capacity; only maintain ghosts (can also just return)
        return;
    }

    // Paper's “constraint”: ensure |T1| + |B1| <= capacity (trim B1 first)
    if (t1_.size() + b1_.size() >= capacity_) {
        if (t1_.size() < capacity_) {
            if (!b1_.empty()) {
                auto tail = b1_.back();
                b1_.pop_back();
                b1_map_.erase(tail);
            }
        } else {
            // |T1| == capacity_; handle via replace per ARC rules
            replace(false);
        }
    } else if (t1_.size() + t2_.size() >= capacity_) {
        // Real cache full: evict one from T1 or T2
        replace(false);
    }

    addToT1MRU(key, value);
}

// ===== Core replacement =====
template <typename Key, typename Value>
void Arc_new<Key, Value>::replace(bool hit_in_b1) {
    // If T1 has surplus (or B1 hit and T1 is at its quota) → evict T1.LRU to B1
    if (!t1_.empty() && (t1_.size() > p_ || (hit_in_b1 && t1_.size() == p_))) {
        evictFromT1ToB1();
    } else {
        // Otherwise evict T2.LRU to B2
        evictFromT2ToB2();
    }
}

template <typename Key, typename Value>
void Arc_new<Key, Value>::evictFromT1ToB1() {
    if (t1_.empty()) return;

    const Key victim = t1_.back();
    t1_.pop_back();

    auto it = map_.find(victim);
    if (it != map_.end()) {
        map_.erase(it);
    }

    // Insert into B1's MRU
    auto iter = attachFront(b1_, victim);
    b1_map_[victim] = iter;

    // Maintain |B1| <= capacity_
    trimGhost(b1_, b1_map_);
}

template <typename Key, typename Value>
void Arc_new<Key, Value>::evictFromT2ToB2() {
    if (t2_.empty()) return;

    const Key victim = t2_.back();
    t2_.pop_back();

    auto it = map_.find(victim);
    if (it != map_.end()) {
        map_.erase(it);
    }

    // Insert into B2's MRU
    auto iter = attachFront(b2_, victim);
    b2_map_[victim] = iter;

    // Maintain |B2| <= capacity_
    trimGhost(b2_, b2_map_);
}

// ===== Adaptive tuning of p =====
template <typename Key, typename Value>
void Arc_new<Key, Value>::adjustPOnB1Hit() {
    // Common approximation: p += max(1, |B2|/|B1|)
    size_t b1s = b1_.size();
    size_t b2s = b2_.size();
    size_t delta = std::max<size_t>(1, (b1s == 0 ? 1 : b2s / b1s));
    p_ = std::min(capacity_, p_ + delta);
}

template <typename Key, typename Value>
void Arc_new<Key, Value>::adjustPOnB2Hit() {
    // Common approximation: p -= max(1, |B1|/|B2|)
    size_t b1s = b1_.size();
    size_t b2s = b2_.size();
    size_t delta = std::max<size_t>(1, (b2s == 0 ? 1 : b1s / b2s));
    p_ = (p_ > delta) ? (p_ - delta) : 0;
}

// ===== List / index operations =====
template <typename Key, typename Value>
void Arc_new<Key, Value>::moveToT2(const Key& key) {
    auto it = map_.find(key);
    if (it == map_.end()) return;

    // Remove from the original list
    if (it->second.tag == ListTag::T1) {
        t1_.erase(it->second.it);
    } else if (it->second.tag == ListTag::T2) {
        t2_.erase(it->second.it);
    }

    // Insert at T2 MRU
    it->second.it = attachFront(t2_, key);
    it->second.tag = ListTag::T2;
}

template <typename Key, typename Value>
void Arc_new<Key, Value>::addToT1MRU(const Key& key, const Value& val) {
    auto iter = attachFront(t1_, key);
    map_[key] = Entry{val, ListTag::T1, iter};
}

template <typename Key, typename Value>
void Arc_new<Key, Value>::addToT2MRU(const Key& key, const Value& val) {
    auto iter = attachFront(t2_, key);
    map_[key] = Entry{val, ListTag::T2, iter};
}

template <typename Key, typename Value>
void Arc_new<Key, Value>::trimGhost(
    std::list<Key>& blist,
    std::unordered_map<Key, typename std::list<Key>::iterator>& bmap) {

    // Constraint: |B1| ≤ capacity_ and |B2| ≤ capacity_
    while (blist.size() > capacity_) {
        auto tail = blist.back();
        blist.pop_back();
        bmap.erase(tail);
    }
}

// Helpers: push-front / erase
template <typename Key, typename Value>
typename std::list<Key>::iterator
Arc_new<Key, Value>::attachFront(std::list<Key>& lst, const Key& key) {
    lst.push_front(key);
    return lst.begin();
}

template <typename Key, typename Value>
void Arc_new<Key, Value>::detach(std::list<Key>& lst, typename std::list<Key>::iterator it) {
    lst.erase(it);
}

} // namespace Cache