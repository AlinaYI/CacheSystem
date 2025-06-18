// ================== CachePolicy.h ==================
// 抽象接口：定义所有缓存策略 (LRU / LFU / ARC …) 必须实现的基本行为
// 1. put  —— 写入 / 更新数据
// 2. get& —— 读取数据（通过引用返回命中值，bool 表示是否命中）
// 3. get  —— 读取数据（直接返回值，未命中可抛异常或返回默认）
//
// 每个具体算法都应继承本类，并实现 3 个纯虚函数
// ===================================================

#pragma once           // 现代写法：保证头文件只被编译一次（等价于传统宏守卫 Include Guard）

namespace Cache {      // 将所有缓存类放进同一命名空间，避免与其他库冲突

// 使用模板使缓存可支持任意 <Key, Value> 类型
template <typename Key, typename Value>
class CachePolicy
{
public:
    // 析构函数必须声明为 virtual，以确保通过基类指针删除派生类时调用正确析构
    virtual ~CachePolicy() = default;

    // ---------- 纯虚函数（pure virtual）----------
    // 没有默认实现，强制所有子类必须自己实现，否则无法实例化
    // ---------------------------------------------

    // 1️⃣ 写入 / 更新缓存
    virtual void put(const Key& key, const Value& value) = 0;

    // 2️⃣ 读取缓存（安全版）
    //    - 若命中：返回 true，并在 value 引用中写入数据
    //    - 若未命中：返回 false
    virtual bool get(const Key& key, Value& value) = 0;

    // 3️⃣ 读取缓存（便捷版）
    //    - 子类实现时通常内部调用上面的 get(Key, Value&)
    //    - 未命中时可选择：
    //        a) 抛出异常   b) 返回默认构造的 Value
    virtual Value get(const Key& key) = 0;
};

} // namespace Cache
