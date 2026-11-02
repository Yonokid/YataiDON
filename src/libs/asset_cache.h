#pragma once
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "../platform/platform.h"

// Every AssetCache registers here so a skin reload can drop them all at once.
class AssetCacheBase {
public:
    virtual void clear() = 0;

protected:
    AssetCacheBase() {
        std::scoped_lock lock(registry_mutex());
        registry().push_back(this);
    }
    ~AssetCacheBase() = default;

private:
    static std::mutex& registry_mutex() {
        static std::mutex mutex;
        return mutex;
    }
    static std::vector<AssetCacheBase*>& registry() {
        static std::vector<AssetCacheBase*> caches;
        return caches;
    }
    friend void clear_asset_caches();
};

// Drops everything retained by every cache. Call while the GPU context is alive.
inline void clear_asset_caches() {
    std::scoped_lock lock(AssetCacheBase::registry_mutex());
    for (AssetCacheBase* cache : AssetCacheBase::registry()) {
        cache->clear();
    }
}

// Bytes a cache may retain: `bytes` where the platform keeps assets between screens, else 0.
inline size_t asset_cache_budget(size_t bytes) {
    return platform_retains_assets() ? bytes : 0;
}

// Thread-safe, byte-bounded retention. Eviction releases only the cache's
// reference; an asset currently used by a scene stays alive. A zero budget retains nothing.
template<class T>
class AssetCache : public AssetCacheBase {
    struct Entry {
        std::shared_ptr<T> value;
        size_t bytes;
        std::list<std::string>::iterator age;
    };
    std::mutex mutex;
    std::list<std::string> order;
    std::unordered_map<std::string, Entry> entries;
    size_t used = 0;
    const size_t budget;

public:
    virtual ~AssetCache() = default;
    explicit AssetCache(size_t budget) : budget(budget) {}
    bool enabled() const { return budget > 0; }
    std::shared_ptr<T> get(const std::string& key) {
        std::scoped_lock lock(mutex);
        auto it = entries.find(key);
        if (it == entries.end()) {
            return {};
        }
        order.splice(order.begin(), order, it->second.age);
        return it->second.value;
    }
    void put(const std::string& key, std::shared_ptr<T> value, size_t bytes) {
        std::scoped_lock lock(mutex);
        auto old = entries.find(key);
        if (old != entries.end()) {
            used -= old->second.bytes;
            order.erase(old->second.age);
            entries.erase(old);
        }
        if (!value || bytes > budget) {
            return;
        }
        while (used > budget - bytes && !order.empty()) {
            auto it = entries.find(order.back());
            used -= it->second.bytes;
            entries.erase(it);
            order.pop_back();
        }
        order.push_front(key);
        entries.emplace(key, Entry{std::move(value), bytes, order.begin()});
        used += bytes;
    }
    void clear() override {
        std::scoped_lock lock(mutex);
        entries.clear();
        order.clear();
        used = 0;
    }
};
