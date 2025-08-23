#pragma once
#include <algorithm>

namespace Cache {

// 插入或更新 key
// 如果已存在，则更新 value 和频率；否则插入新节点，并可能驱逐
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

    // 维护全局统计
    curTotalNum_ += 1; // 初始 freq=1
    curAverageNum_ = nodeMap_.empty() ? 0 : (curTotalNum_ / static_cast<int>(nodeMap_.size()));

    maybeAge();
}

// 获取 key 对应的 value，如果存在则返回 true 且提升频率，否则 false
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

// 将节点提升到 freq+1 的链表中
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

    // 更新全局统计（总频次 +1；平均值重算）
    curTotalNum_ += 1;
    curAverageNum_ = nodeMap_.empty() ? 0 : (curTotalNum_ / static_cast<int>(nodeMap_.size()));
}

// 淘汰当前最小频率的链表中的最旧节点（链表头部）
template<typename Key, typename Value>
void LfuCache<Key, Value>::evict() {
    if (nodeMap_.empty()) return;
    if (freqMap_.count(minFreq_) == 0 || !freqMap_[minFreq_]) {
        // 保险起见，寻找当前存在的最小频率
        updateMinFreq();
        if (freqMap_.count(minFreq_) == 0 || !freqMap_[minFreq_]) return;
    }

    auto node = freqMap_[minFreq_]->getFirstNode();
    if (!node || node->next_ == nullptr) return; // 空或仅哨兵，容错

    const int removedFreq = node->freq;

    freqMap_[minFreq_]->removeNode(node);
    if (freqMap_[minFreq_]->isEmpty()) {
        freqMap_.erase(minFreq_);
        updateMinFreq();
    }

    nodeMap_.erase(node->key);

    // 减掉该节点贡献的频次
    curTotalNum_ -= removedFreq;
    curTotalNum_ = std::max(0, curTotalNum_);
    curAverageNum_ = nodeMap_.empty() ? 0 : (curTotalNum_ / static_cast<int>(nodeMap_.size()));
}

template<typename Key, typename Value>
void LfuCache<Key, Value>::updateMinFreq() {
    // 简单地线性找一个最小存在的频率；Aging 后通常会回到 1
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
        // 没有有效桶
        minFreq_ = 1;
    } else {
        minFreq_ = candidate;
    }
}

// ===================== Aging 实现 =====================

template<typename Key, typename Value>
void LfuCache<Key, Value>::maybeAge() {
    if (!nodeMap_.empty() && curAverageNum_ > maxAverageNum_) {
        ageAll();
    }
}

template<typename Key, typename Value>
void LfuCache<Key, Value>::ageAll() {
    // 把所有节点的频次统一“减半”，最低为 1
    // 1) 取出全部节点
    std::vector<NodePtr> all;
    all.reserve(nodeMap_.size());

    for (auto& kv : freqMap_) {
        auto& listPtr = kv.second;
        if (!listPtr || listPtr->isEmpty()) continue;
        listPtr->extractAll(all);
    }
    freqMap_.clear();

    // 2) 重新分桶 & 重新统计总频次
    long long newTotal = 0;
    for (auto& node : all) {
        const int oldFreq = node->freq;
        int newFreq = (oldFreq > 1) ? (oldFreq / 2) : 1; // 减半衰减；如需“减一”可改：std::max(1, oldFreq - 1)
        if (newFreq < 1) newFreq = 1;
        node->freq = newFreq;

        if (!freqMap_[newFreq]) {
            freqMap_[newFreq] = std::make_unique<FreqList<Key, Value>>(newFreq);
        }
        freqMap_[newFreq]->addNode(node);
        newTotal += newFreq;
    }

    // 3) 重新计算 minFreq_ / curTotalNum_ / curAverageNum_
    minFreq_ = 1;  // 衰减后最小频次回到 1
    curTotalNum_ = static_cast<int>(newTotal);
    curAverageNum_ = nodeMap_.empty() ? 0 : (curTotalNum_ / static_cast<int>(nodeMap_.size()));
}

} // namespace Cache
