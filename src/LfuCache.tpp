#pragma once
#include <algorithm>
#include "../include/LfuCache.h"

namespace Cache {

// Insert or update key
// If it exists, update value and frequency; otherwise insert a new node and possibly evict
template<typename Key, typename Value>
void LfuCache<Key, Value>::put(const Key& key, const Value& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (capacity_ == 0) return;

    auto it = nodeMap_.find(key);
    if (it != nodeMap_.end()) {
        it->second->value = value;
        increaseFrequency(it->second);
        maybeAge();
        return;
    }

    if (nodeMap_.size() >= static_cast<size_t>(capacity_)) {
        evict();
    }

    auto node = std::make_shared<typename FreqList<Key, Value>::Node>(key, value);
    nodeMap_[key] = node;

    if (!freqMap_[1]) freqMap_[1] = std::make_unique<FreqList<Key, Value>>(1);
    freqMap_[1]->addNode(node);
    minFreq_ = 1;

    // Maintain global statistics
    curTotalNum_ += 1; // Initial freq=1
    curAverageNum_ = nodeMap_.empty() ? 0 : (curTotalNum_ / static_cast<int>(nodeMap_.size()));

    maybeAge();
}

// Get value corresponding to key, return true and increase frequency if it exists, otherwise false
template<typename Key, typename Value>
bool LfuCache<Key, Value>::get(const Key& key, Value& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = nodeMap_.find(key);
    if (it == nodeMap_.end()) return false;

    value = it->second->value;
    increaseFrequency(it->second);
    maybeAge();
    return true;
}

// Move node to freq+1 list
template<typename Key, typename Value>
void LfuCache<Key, Value>::increaseFrequency(NodePtr node) {
    const int oldFreq = node->freq;

    auto itList = freqMap_.find(oldFreq);
    if (itList != freqMap_.end() && itList->second) {
        itList->second->removeNode(node);
        if (itList->second->isEmpty()) {
            freqMap_.erase(oldFreq);
            if (minFreq_ == oldFreq) {
                minFreq_ = oldFreq + 1;
            }
        }
    }

    node->freq = oldFreq + 1;
    const int newFreq = node->freq;

    if (!freqMap_[newFreq]) {
        freqMap_[newFreq] = std::make_unique<FreqList<Key, Value>>(newFreq);
    }
    freqMap_[newFreq]->addNode(node);

    // Update global statistics (total frequency +1; recompute average)
    curTotalNum_ += 1;
    curAverageNum_ = nodeMap_.empty() ? 0 : (curTotalNum_ / static_cast<int>(nodeMap_.size()));
}

// Evict the oldest node in the list with the current minimum frequency (list head)
template<typename Key, typename Value>
void LfuCache<Key, Value>::evict() {
    if (nodeMap_.empty()) return;
    if (freqMap_.count(minFreq_) == 0 || !freqMap_[minFreq_]) {
        // For safety, find the current minimum frequency
        updateMinFreq();
        if (freqMap_.count(minFreq_) == 0 || !freqMap_[minFreq_]) return;
    }

    auto node = freqMap_[minFreq_]->getFirstNode();
    if (!node || node->next_ == nullptr) return; // Empty or only sentinel, for safety

    const int removedFreq = node->freq;

    freqMap_[minFreq_]->removeNode(node);
    if (freqMap_[minFreq_]->isEmpty()) {
        freqMap_.erase(minFreq_);
        updateMinFreq();
    }

    nodeMap_.erase(node->key);

    // Subtract the frequency contributed by this node
    curTotalNum_ -= removedFreq;
    curTotalNum_ = std::max(0, curTotalNum_);
    curAverageNum_ = nodeMap_.empty() ? 0 : (curTotalNum_ / static_cast<int>(nodeMap_.size()));
}

template<typename Key, typename Value>
void LfuCache<Key, Value>::updateMinFreq() {
    // Simply find the minimum frequency that exists; after Aging, it usually returns to 1
    if (freqMap_.empty()) {
        minFreq_ = 1;
        return;
    }
    int candidate = std::numeric_limits<int>::max();
    for (const auto& kv : freqMap_) {
        if (kv.second && !kv.second->isEmpty()) {
            candidate = std::min(candidate, kv.first);
        }
    }
    if (candidate == std::numeric_limits<int>::max()) {
        // No valid bucket
        minFreq_ = 1;
    } else {
        minFreq_ = candidate;
    }
}

// ===================== Aging implementation =====================

template<typename Key, typename Value>
void LfuCache<Key, Value>::maybeAge() {
    if (!nodeMap_.empty() && curAverageNum_ > maxAverageNum_) {
        ageAll();
    }
}

template<typename Key, typename Value>
void LfuCache<Key, Value>::ageAll() {
    // Halve the frequency of all nodes, minimum is 1
    // 1) Extract all nodes
    std::vector<NodePtr> all;
    all.reserve(nodeMap_.size());

    for (auto& kv : freqMap_) {
        auto& listPtr = kv.second;
        if (!listPtr || listPtr->isEmpty()) continue;
        listPtr->extractAll(all);
    }
    freqMap_.clear();

    // 2) Re-bucket & re-count total frequency
    long long newTotal = 0;
    for (auto& node : all) {
        const int oldFreq = node->freq;
        int newFreq = (oldFreq > 1) ? (oldFreq / 2) : 1; // Half-decay; if "subtract 1" is needed: std::max(1, oldFreq - 1)
        if (newFreq < 1) newFreq = 1;
        node->freq = newFreq;

        if (!freqMap_[newFreq]) {
            freqMap_[newFreq] = std::make_unique<FreqList<Key, Value>>(newFreq);
        }
        freqMap_[newFreq]->addNode(node);
        newTotal += newFreq;
    }

    // 3) Re-compute minFreq_ / curTotalNum_ / curAverageNum_
    minFreq_ = 1;  // After decay, the minimum frequency returns to 1
    curTotalNum_ = static_cast<int>(newTotal);
    curAverageNum_ = nodeMap_.empty() ? 0 : (curTotalNum_ / static_cast<int>(nodeMap_.size()));
}

} // namespace Cache
