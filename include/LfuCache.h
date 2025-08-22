#pragma once

#include <unordered_map>
#include <memory>
#include <mutex>
#include <limits>
#include <list>
#include <vector>
#include "CachePolicy.h"

namespace Cache {

// =========================================================
// FreqList: 双向链表结构，用于按频率组织 LFU 缓存节点
// =========================================================

template<typename Key, typename Value>
class FreqList {
public:
    struct Node {
        int freq;
        Key key;
        Value value;

        std::weak_ptr<Node> prev_;
        std::shared_ptr<Node> next_;

        Node() : freq(1), next_(nullptr) {}

        Node(Key k, Value v)
            : freq(1), key(std::move(k)), value(std::move(v)), next_(nullptr) {}
    };

public:
    using NodePtr = std::shared_ptr<Node>;

    explicit FreqList(int freq) : freq_(freq) {
        head_ = std::make_shared<Node>();
        tail_ = std::make_shared<Node>();
        head_->next_ = tail_;
        tail_->prev_ = head_;
    }

    bool isEmpty() const {
        return head_->next_ == tail_;
    }

    void addNode(NodePtr node) {
        if (!node || !tail_ || tail_->prev_.expired()) return;
        auto prev = tail_->prev_.lock();
        node->next_ = tail_;
        node->prev_ = prev;
        prev->next_ = node;
        tail_->prev_ = node;
    }

    void removeNode(NodePtr node) {
        if (!node || node->prev_.expired() || !node->next_) return;
        auto prev = node->prev_.lock();
        auto next = node->next_;
        if (prev) prev->next_ = next;
        if (next) next->prev_ = prev;
        node->next_ = nullptr;
        node->prev_.reset();
    }

    NodePtr getFirstNode() const {
        return head_->next_;
    }

    // 把本链表的“真实节点”全部取出，并清空本链表
    void extractAll(std::vector<NodePtr>& out) {
        auto cur = head_->next_;
        while (cur && cur != tail_) {
            out.push_back(cur);
            cur = cur->next_;
        }
        // 清空链表（保留哨兵）
        head_->next_ = tail_;
        tail_->prev_ = head_;
    }

    int freq() const { return freq_; }

private:
    int freq_;
    NodePtr head_;
    NodePtr tail_;
};


// =========================================================
/* LFU Cache 类定义（带 Aging）
 * 触发条件：curAverageNum_ > maxAverageNum_
 * 衰减策略：所有节点 freq = max(1, freq / 2)
 */
// =========================================================

template<typename Key, typename Value>
class LfuCache : public CachePolicy<Key, Value> {
public:
    using NodePtr = typename FreqList<Key, Value>::NodePtr;

    // capacity: 容量；maxAvg: 触发 Aging 的平均频次阈值
    LfuCache(int capacity, int maxAvg = 1000000)
        : capacity_(capacity),
          maxAverageNum_(maxAvg),
          minFreq_(1),
          curAverageNum_(0),
          curTotalNum_(0) {}

    void put(const Key& key, const Value& value) override;
    bool get(const Key& key, Value& value) override;

    // 方便使用者的便捷版（未命中返回 Value()）
    Value get(const Key& key) override {
        Value v{};
        return get(key, v) ? v : Value{};
    }

    void purge() {
        std::lock_guard<std::mutex> lock(mutex_);
        nodeMap_.clear();
        freqMap_.clear();
        minFreq_ = 1;
        curAverageNum_ = 0;
        curTotalNum_ = 0;
    }

private:
    void increaseFrequency(NodePtr node);
    void evict();
    void updateMinFreq(); // 可选：当前实现未显式使用，但保留声明便于扩展

    // Aging 相关
    void maybeAge();  // 判断是否需要触发衰减
    void ageAll();    // 执行一次全量衰减

private:
    int capacity_;
    int maxAverageNum_;
    int minFreq_;
    int curAverageNum_;
    int curTotalNum_;

    std::mutex mutex_;
    std::unordered_map<Key, NodePtr> nodeMap_;
    std::unordered_map<int, std::unique_ptr<FreqList<Key, Value>>> freqMap_;
};

} // namespace Cache

#include "LfuCache.tpp"