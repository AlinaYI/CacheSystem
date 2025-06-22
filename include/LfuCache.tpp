#include "LfuCache.h"
#include <iostream>

namespace Cache {

// 插入或更新 key
// 如果已存在，则更新 value 和频率；否则插入新节点，并可能驱逐
// get() 也调用 increaseFrequency() 提升频率

template<typename Key, typename Value>
void LfuCache<Key, Value>::put(const Key& key, const Value& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (capacity_ == 0) return;

    auto it = nodeMap_.find(key);
    if (it != nodeMap_.end()) {
        it->second->value = value;
        increaseFrequency(it->second);
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
    curTotalNum_++;
    curAverageNum_ = curTotalNum_ / nodeMap_.size();
}

// 获取 key 对应的 value，如果存在则返回 true 且提升频率，否则 false
template<typename Key, typename Value>
bool LfuCache<Key, Value>::get(const Key& key, Value& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = nodeMap_.find(key);
    if (it == nodeMap_.end()) return false;

    value = it->second->value;
    increaseFrequency(it->second);
    return true;
}

// 将节点提升到 freq+1 的链表中
template<typename Key, typename Value>
void LfuCache<Key, Value>::increaseFrequency(NodePtr node) {
    int oldFreq = node->freq;
    freqMap_[oldFreq]->removeNode(node);
    if (freqMap_[oldFreq]->isEmpty()) {
        freqMap_.erase(oldFreq);
        if (minFreq_ == oldFreq) minFreq_++;
    }

    node->freq++;
    int newFreq = node->freq;
    if (!freqMap_[newFreq]) freqMap_[newFreq] = std::make_unique<FreqList<Key, Value>>(newFreq);
    freqMap_[newFreq]->addNode(node);

    curTotalNum_++;
    curAverageNum_ = curTotalNum_ / nodeMap_.size();
}

// 淘汰当前最小频率的链表中的最旧节点
template<typename Key, typename Value>
void LfuCache<Key, Value>::evict() {
    if (freqMap_.count(minFreq_) == 0) return;
    auto node = freqMap_[minFreq_]->getFirstNode();
    freqMap_[minFreq_]->removeNode(node);
    if (freqMap_[minFreq_]->isEmpty()) {
        freqMap_.erase(minFreq_);
    }
    nodeMap_.erase(node->key);
    curTotalNum_ -= node->freq;
    curAverageNum_ = nodeMap_.empty() ? 0 : curTotalNum_ / nodeMap_.size();
}

} // namespace Cache
