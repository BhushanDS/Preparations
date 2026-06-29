#pragma once

#include "kvstore/common.hpp"
#include "kvstore/sharded_map.hpp"
#include "kvstore/ttl_manager.hpp"
#include "kvstore/wal.hpp"
#include "kvstore/snapshot.hpp"
#include "kvstore/protocol.hpp"
#include <spdlog/spdlog.h>

namespace kvstore {

struct StoreConfig {
    std::filesystem::path dataDir = "./kvdata";
    Duration ttlScanInterval = std::chrono::seconds(1);
    Duration snapshotInterval = std::chrono::minutes(5);
    bool enablePersistence = true;
};

class KVStore {
    static constexpr size_t NUM_SHARDS = 64;
    
public:
    explicit KVStore(StoreConfig config = {})
        : config_(std::move(config)),
          ttlManager_(map_, config_.ttlScanInterval),
          wal_(config_.dataDir / "wal.log"),
          snapshotMgr_(config_.dataDir / "snapshots") {}
    
    // Initialize: load snapshot, replay WAL, start background tasks
    Result<> open() {
        spdlog::info("Opening KVStore (data dir: {})", config_.dataDir.string());
        
        std::filesystem::create_directories(config_.dataDir);
        
        if (config_.enablePersistence) {
            // 1. Load latest snapshot
            auto loadResult = snapshotMgr_.load(map_);
            if (!loadResult) {
                spdlog::error("Failed to load snapshot");
                return std::unexpected(loadResult.error());
            }
            spdlog::info("Loaded {} entries from snapshot", *loadResult);
            
            // 2. Replay WAL (entries after the snapshot)
            auto walResult = wal_.readAll();
            if (!walResult) {
                spdlog::error("Failed to read WAL");
                return std::unexpected(walResult.error());
            }
            
            size_t replayed = 0;
            for (const auto& entry : *walResult) {
                replayWALEntry(entry);
                ++replayed;
            }
            spdlog::info("Replayed {} WAL entries", replayed);
            
            // 3. Open WAL for new writes
            auto openResult = wal_.open();
            if (!openResult) {
                return std::unexpected(openResult.error());
            }
        }
        
        // 4. Start TTL manager
        ttlManager_.start();
        
        // 5. Start snapshot timer
        if (config_.enablePersistence) {
            startSnapshotTimer();
        }
        
        spdlog::info("KVStore ready ({} keys)", map_.size());
        return {};
    }
    
    // Execute a parsed command and return response string
    std::string execute(const Command& cmd) {
        switch (cmd.type) {
            case CommandType::GET:
                return executeGet(cmd);
            case CommandType::SET:
                return executeSet(cmd);
            case CommandType::DEL:
                return executeDel(cmd);
            case CommandType::EXISTS:
                return executeExists(cmd);
            case CommandType::KEYS:
                return executeKeys(cmd);
            case CommandType::EXPIRE:
                return executeExpire(cmd);
            case CommandType::TTL:
                return executeTTL(cmd);
            case CommandType::PING:
                return Protocol::pong();
            case CommandType::QUIT:
                return Protocol::ok();
            default:
                return Protocol::error("unknown command");
        }
    }
    
    void close() {
        spdlog::info("Closing KVStore...");
        
        // Stop background tasks
        ttlManager_.stop();
        if (snapshotWorker_.joinable()) {
            snapshotWorker_.request_stop();
            snapshotWorker_.join();
        }
        
        // Final snapshot + WAL close
        if (config_.enablePersistence) {
            snapshotMgr_.save(map_);
            wal_.close();
        }
        
        spdlog::info("KVStore closed");
    }
    
    ~KVStore() { close(); }
    
    // Direct API (for embedded use without networking)
    std::optional<std::string> get(const std::string& key) { return map_.get(key); }
    void set(const std::string& key, std::string value, std::optional<Duration> ttl = {}) {
        Entry entry{.value = std::move(value)};
        if (ttl) entry.expiry = Clock::now() + *ttl;
        map_.set(key, std::move(entry));
    }
    bool del(const std::string& key) { return map_.del(key); }
    size_t size() const { return map_.size(); }
    
private:
    StoreConfig config_;
    ShardedMap<NUM_SHARDS> map_;
    TTLManager<NUM_SHARDS> ttlManager_;
    WriteAheadLog wal_;
    SnapshotManager snapshotMgr_;
    std::jthread snapshotWorker_;
    
    // --- Command Handlers ---
    
    std::string executeGet(const Command& cmd) {
        if (cmd.args.size() != 1) {
            return Protocol::error("GET requires exactly 1 argument");
        }
        auto value = map_.get(cmd.args[0]);
        return value ? Protocol::bulkString(*value) : Protocol::nullBulk();
    }
    
    std::string executeSet(const Command& cmd) {
        if (cmd.args.size() < 2 || cmd.args.size() > 4) {
            return Protocol::error("SET key value [EX seconds]");
        }
        
        Entry entry{.value = cmd.args[1]};
        int64_t ttlMs = 0;
        
        // Optional: SET key value EX 60
        if (cmd.args.size() == 4 && (cmd.args[2] == "EX" || cmd.args[2] == "ex")) {
            auto seconds = Protocol::parseInt(cmd.args[3]);
            if (!seconds || *seconds <= 0) {
                return Protocol::error("invalid expire time");
            }
            entry.expiry = Clock::now() + std::chrono::seconds(*seconds);
            ttlMs = *seconds * 1000;
        }
        
        map_.set(cmd.args[0], std::move(entry));
        
        // WAL
        if (config_.enablePersistence) {
            wal_.append({.op = WALOp::SET, .key = cmd.args[0], 
                         .value = cmd.args[1], .ttlMs = ttlMs});
        }
        
        return Protocol::ok();
    }
    
    std::string executeDel(const Command& cmd) {
        if (cmd.args.empty()) {
            return Protocol::error("DEL requires at least 1 argument");
        }
        
        int64_t deleted = 0;
        for (const auto& key : cmd.args) {
            if (map_.del(key)) {
                ++deleted;
                if (config_.enablePersistence) {
                    wal_.append({.op = WALOp::DEL, .key = key});
                }
            }
        }
        
        return Protocol::integer(deleted);
    }
    
    std::string executeExists(const Command& cmd) {
        if (cmd.args.size() != 1) {
            return Protocol::error("EXISTS requires exactly 1 argument");
        }
        return Protocol::integer(map_.exists(cmd.args[0]) ? 1 : 0);
    }
    
    std::string executeKeys(const Command& cmd) {
        std::string pattern = (cmd.args.size() >= 1) ? cmd.args[0] : "*";
        auto keys = map_.keys(pattern);
        return Protocol::array(keys);
    }
    
    std::string executeExpire(const Command& cmd) {
        if (cmd.args.size() != 2) {
            return Protocol::error("EXPIRE key seconds");
        }
        
        auto seconds = Protocol::parseInt(cmd.args[1]);
        if (!seconds || *seconds <= 0) {
            return Protocol::error("invalid expire time");
        }
        
        auto ttl = std::chrono::seconds(*seconds);
        bool ok = map_.expire(cmd.args[0], ttl);
        
        if (ok && config_.enablePersistence) {
            wal_.append({.op = WALOp::EXPIRE, .key = cmd.args[0],
                         .ttlMs = *seconds * 1000});
        }
        
        return Protocol::integer(ok ? 1 : 0);
    }
    
    std::string executeTTL(const Command& cmd) {
        if (cmd.args.size() != 1) {
            return Protocol::error("TTL requires exactly 1 argument");
        }
        
        auto remaining = map_.ttl(cmd.args[0]);
        if (!remaining) {
            return Protocol::integer(-2);  // Key doesn't exist
        }
        if (*remaining == Duration::max()) {
            return Protocol::integer(-1);  // No expiry set
        }
        
        auto seconds = std::chrono::duration_cast<std::chrono::seconds>(*remaining).count();
        return Protocol::integer(seconds);
    }
    
    // --- WAL Replay ---
    
    void replayWALEntry(const WALEntry& entry) {
        switch (entry.op) {
            case WALOp::SET: {
                Entry e{.value = entry.value};
                if (entry.ttlMs > 0) {
                    e.expiry = Clock::now() + std::chrono::milliseconds(entry.ttlMs);
                }
                map_.set(entry.key, std::move(e));
                break;
            }
            case WALOp::DEL:
                map_.del(entry.key);
                break;
            case WALOp::EXPIRE:
                if (entry.ttlMs > 0) {
                    map_.expire(entry.key, std::chrono::milliseconds(entry.ttlMs));
                }
                break;
        }
    }
    
    // --- Snapshot Timer ---
    
    void startSnapshotTimer() {
        snapshotWorker_ = std::jthread([this](std::stop_token token) {
            while (!token.stop_requested()) {
                std::this_thread::sleep_for(config_.snapshotInterval);
                if (token.stop_requested()) break;
                
                spdlog::info("Taking periodic snapshot...");
                auto result = snapshotMgr_.save(map_);
                if (result) {
                    wal_.truncate();  // WAL can be truncated after snapshot
                } else {
                    spdlog::error("Snapshot failed!");
                }
            }
        });
    }
};

} // namespace kvstore
