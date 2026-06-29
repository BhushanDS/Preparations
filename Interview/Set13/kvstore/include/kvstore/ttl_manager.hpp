#pragma once

#include "kvstore/common.hpp"
#include "kvstore/sharded_map.hpp"
#include <thread>
#include <spdlog/spdlog.h>

namespace kvstore {

// Two strategies for key expiration:
// 1. LAZY: Check TTL on every GET (done in ShardedMap::get)
// 2. ACTIVE: Background thread periodically scans for expired keys
// Redis uses the same dual strategy

template<size_t NumShards = 64>
class TTLManager {
public:
    explicit TTLManager(ShardedMap<NumShards>& map, 
                        Duration scanInterval = std::chrono::seconds(1))
        : map_(map), scanInterval_(scanInterval) {}
    
    void start() {
        running_ = true;
        worker_ = std::jthread([this](std::stop_token token) {
            spdlog::info("TTL manager started (scan interval: {}ms)",
                std::chrono::duration_cast<std::chrono::milliseconds>(scanInterval_).count());
            
            while (!token.stop_requested()) {
                std::this_thread::sleep_for(scanInterval_);
                
                auto purged = map_.purgeExpired();
                if (purged > 0) {
                    spdlog::debug("TTL purge: removed {} expired keys", purged);
                }
            }
            spdlog::info("TTL manager stopped");
        });
    }
    
    void stop() {
        if (worker_.joinable()) {
            worker_.request_stop();
            worker_.join();
        }
        running_ = false;
    }
    
    ~TTLManager() { stop(); }
    
private:
    ShardedMap<NumShards>& map_;
    Duration scanInterval_;
    std::jthread worker_;
    bool running_ = false;
};

} // namespace kvstore
