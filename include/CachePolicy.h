// ================== CachePolicy.h ==================
// Abstract interface: defines the basic behavior that all cache policies
// (LRU / LFU / ARC …) must implement
// 1. put  —— write / update data
// 2. get& —— read data (return hit value by reference; bool indicates hit/miss)
// 3. get  —— read data (return value directly; on miss either throw or return default)
//
// Every concrete algorithm should inherit from this class and implement
// these 3 pure virtual functions.
// ===================================================

#pragma once           // Modern form: ensure the header is compiled only once (equivalent to an include guard)

namespace Cache {      // Put all cache classes in the same namespace to avoid clashes with other libraries

// Use templates so the cache supports arbitrary <Key, Value> types
template <typename Key, typename Value>
class CachePolicy
{
public:
    // The destructor must be virtual to ensure the correct destructor is called
    // when deleting a derived object via a base-class pointer.
    virtual ~CachePolicy() = default;

    // ---------- Pure virtual functions ----------
    // No default implementation; all derived classes must provide one,
    // otherwise they cannot be instantiated.
    // --------------------------------------------

    // 1️⃣ Write / update the cache
    virtual void put(const Key& key, const Value& value) = 0;

    // 2️⃣ Read from the cache (safe version)
    //    - On hit: return true and write the value to the output reference
    //    - On miss: return false
    virtual bool get(const Key& key, Value& value) = 0;

    // 3️⃣ Read from the cache (convenience version)
    //    - Implementations typically call the above get(Key, Value&) internally
    //    - On miss, you can choose to:
    //        a) throw an exception   b) return a default-constructed Value
    virtual Value get(const Key& key) = 0;
};

} // namespace Cache
