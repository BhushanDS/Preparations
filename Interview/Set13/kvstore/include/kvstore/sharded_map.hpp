#pragma once

#include "kvstore/common.hpp"
#include <array>
#include <functional>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>

namespace kvstore {

// Sharding strategy: hash the key, mod by shard count
// Each shard has its own mutex - minimal contention
// With 64 shards and 8 threads: ~12.5% chance of contention per op

template<size_t NumShards = 64>
class ShardedMap {
public:
    // SET - insert or update
    void set(const std::string& key, Entry entry) {
        auto& shard = getShard(key);
        std::unique_lock lock(shard.mutex);
        shard.data[key] = std::move(entry);
    }
    
    // GET - returns copy of value (safe after lock release)
    std::optional<std::string> get(const std::string& key) {
        auto& shard = getShard(key);
        std::shared_lock lock(shard.mutex);  // Reader lock - multiple concurrent reads
        
        auto it = shard.data.find(key);
        if (it == shard.data.end()) return std::nullopt;
        
        // Check TTL (lazy expiration)
        if (it->second.isExpired()) {
            lock.unlock();
            // Upgrade to writer lock for deletion
            std::unique_lock writeLock(shard.mutex);
            it = shard.data.find(key);
            if (it != shard.data.end() && it->second.isExpired()) {
                shard.data.erase(it);
            }
            return std::nullopt;
        }
        
        return it->second.value;
    }
    
    // DEL - returns true if key existed
    bool del(const std::string& key) {
        auto& shard = getShard(key);
        std::unique_lock lock(shard.mutex);
        return shard.data.erase(key) > 0;
    }
    
    // EXISTS - check if key exists and not expired
    bool exists(const std::string& key) {
        auto& shard = getShard(key);
        std::shared_lock lock(shard.mutex);
        auto it = shard.data.find(key);
        if (it == shard.data.end()) return false;
        return !it->second.isExpired();
    }
    
    // EXPIRE - set TTL on existing key
    bool expire(const std::string& key, Duration ttl) {
        auto& shard = getShard(key);
        std::unique_lock lock(shard.mutex);
        auto it = shard.data.find(key);
        if (it == shard.data.end()) return false;
        it->second.expiry = Clock::now() + ttl;
        return true;
    }
    
    // TTL - get remaining time-to-live
    std::optional<Duration> ttl(const std::string& key) {
        auto& shard = getShard(key);
        std::shared_lock lock(shard.mutex);
        auto it = shard.data.find(key);
        if (it == shard.data.end()) return std::nullopt;
        if (!it->second.expiry) return Duration::max();  // No expiry
        
        auto remaining = *it->second.expiry - Clock::now();
        if (remaining <= Duration::zero()) return Duration::zero();
        return remaining;
    }
    
    // KEYS - return all non-expired keys matching pattern
    // WARNING: O(N) - scans all shards, holds locks sequentially
    std::vector<std::string> keys(std::string_view pattern = "*") {
        std::vector<std::string> result;
        for (auto& shard : shards_) {
            std::shared_lock lock(shard.mutex);
            for (const auto& [key, entry] : shard.data) {
                if (!entry.isExpired() && matchPattern(key, pattern)) {
                    result.push_back(key);
                }
            }
        }
        return result;
    }
    
    // Size - approximate count (not atomic across shards)
    size_t size() const {
        size_t total = 0;
        for (const auto& shard : shards_) {
            std::shared_lock lock(shard.mutex);
            total += shard.data.size();
        }
        return total;
    }
    
    // Iterate all entries (for snapshots) - caller must handle thread safety
    template<typename Func>
    void forEach(Func&& fn) {
        for (auto& shard : shards_) {
            std::shared_lock lock(shard.mutex);
            for (const auto& [key, entry] : shard.data) {
                fn(key, entry);
            }
        }
    }
    
    // Remove all expired entries (active expiration)
    size_t purgeExpired() {
        size_t purged = 0;
        auto now = Clock::now();
        for (auto& shard : shards_) {
            std::unique_lock lock(shard.mutex);
            for (auto it = shard.data.begin(); it != shard.data.end(); ) {
                if (it->second.expiry && now > *it->second.expiry) {
                    it = shard.data.erase(it);
                    ++purged;
                } else {
                    ++it;
                }
            }
        }
        return purged;
    }
    
    void clear() {
        for (auto& shard : shards_) {
            std::unique_lock lock(shard.mutex);
            shard.data.clear();
        }
    }
    
private:
    struct Shard {
        mutable std::shared_mutex mutex;
        std::unordered_map<std::string, Entry> data;
    };
    
    std::array<Shard, NumShards> shards_;
    
    Shard& getShard(const std::string& key) {
        size_t hash = std::hash<std::string>{}(key);
        return shards_[hash % NumShards];
    }
    
    const Shard& getShard(const std::string& key) const {
        size_t hash = std::hash<std::string>{}(key);
        return shards_[hash % NumShards];
    }
    
    // Simple glob pattern matching (* only)
    static bool matchPattern(const std::string& str, std::string_view pattern) {
        if (pattern == "*") return true;
        
        // Simple prefix match: "user:*" matches "user:alice"
        if (pattern.ends_with('*')) {
            auto prefix = pattern.substr(0, pattern.size() - 1);
            return str.starts_with(prefix);
        }
        
        return str == pattern;
    }
};

} // namespace kvstore
