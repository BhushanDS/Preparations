# Set 13: Mini-Project -> Production-Quality Concurrent Key-Value Store

> **The Test**: "Build a networked key-value store in C++20 in 90 minutes."
> This is the kind of take-home or live system design + coding exercise given to 10+ year candidates at companies like Google, Meta, Bloomberg, Jane Street, and Citadel.
> It tests: Modern C++, concurrency, networking, persistence, testing, build systems, error handling, and API design -> all at once.

---



## Project Overview

### What We're Building
```
+--------------------------------------------------------------+
-                    KVStore Server                            ?
-                                                              ?
|  +---------+    +--------------+    +--------------------+  |
|  |  TCP     +----+  Command     +----+  KVStore Engine    ->  |
|  |  Server  ->    |  Parser      ->    |                    |  |
|  |  (Asio)  |    |  (Protocol)  |    |  +--------------+ |  |
|  |          +----+              +----+  | ShardedMap   -> |  |
|  | async    ->    |  GET/SET/DEL ->    |  | (lock-free   -> |  |
|  | per-conn ->    |  KEYS/TTL   ->    |  |  per-shard)  | |  |
|  | coroutine|    |  EXPIRE     ->    |  +--------------+ |  |
|  +---------+    +--------------+    |  +--------------+ |  |
-                                      |  | TTL Manager  -> |  |
|  +-----------------------------+    |  | (lazy+active)| |  |
|  |  Write-Ahead Log (WAL)      |    |  +--------------+ |  |
|  |  (crash recovery)           +----+  +--------------+ |  |
|  |  append-only file           ->    |  | Snapshot     -> |  |
|  +-----------------------------+    |  | (periodic)   | |  |
-                                      |  +--------------+ |  |
-                                      +--------------------+  |
+--------------------------------------------------------------+
```

### Features
- **GET / SET / DEL / KEYS / EXISTS** | core operations
- **TTL / EXPIRE** | key expiration (lazy + active eviction)
- **Thread-safe** | sharded map with per-shard locking (minimal contention)
- **Persistence** | Write-Ahead Log (WAL) + periodic snapshots
- **Networking** | async TCP server with text protocol (Redis-like)
- **Crash recovery** | replay WAL on startup
- **Benchmarks** | throughput and latency measurement

---

## Step 1: Project Structure & Build System

### Directory Layout
```
kvstore/
-?? CMakeLists.txt              # Build configuration
-?? vcpkg.json                  # Package manager dependencies
-?? include/
-   --> kvstore/
-       --> store.hpp           # Core KVStore engine
-       --> sharded_map.hpp     # Thread-safe sharded hash map
-       --> ttl_manager.hpp     # TTL expiration logic
-       --> wal.hpp             # Write-Ahead Log
-       --> snapshot.hpp        # Snapshot persistence
-       --> server.hpp          # TCP server
-       --> protocol.hpp        # Command parser
-       --> common.hpp          # Shared types
-?? src/
-   --> store.cpp
-   --> sharded_map.cpp
-   --> ttl_manager.cpp
-   --> wal.cpp
-   --> snapshot.cpp
-   --> server.cpp
-   --> protocol.cpp
-   --> main.cpp                # Entry point
-?? tests/
-   --> test_store.cpp
-   --> test_sharded_map.cpp
-   --> test_protocol.cpp
-   --> test_wal.cpp
-   --> test_integration.cpp
-?? bench/
    -?? bench_store.cpp         # Google Benchmark
```

### CMakeLists.txt
```cmake
cmake_minimum_required(VERSION 3.20)
project(kvstore VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Dependencies
find_package(Boost REQUIRED COMPONENTS system)
find_package(GTest REQUIRED)
find_package(benchmark REQUIRED)
find_package(spdlog REQUIRED)

# === Core library ===
add_library(kvstore_lib
    src/store.cpp
    src/sharded_map.cpp
    src/ttl_manager.cpp
    src/wal.cpp
    src/snapshot.cpp
    src/server.cpp
    src/protocol.cpp
)
target_include_directories(kvstore_lib PUBLIC include)
target_link_libraries(kvstore_lib PUBLIC 
    Boost::system
    spdlog::spdlog
    pthread
)
target_compile_options(kvstore_lib PRIVATE
    -Wall -Wextra -Wpedantic -Werror
    -Wshadow -Wnon-virtual-dtor -Wconversion
)

# === Executable ===
add_executable(kvstore src/main.cpp)
target_link_libraries(kvstore PRIVATE kvstore_lib)

# === Tests ===
enable_testing()
add_executable(kvstore_tests
    tests/test_store.cpp
    tests/test_sharded_map.cpp
    tests/test_protocol.cpp
    tests/test_wal.cpp
    tests/test_integration.cpp
)
target_link_libraries(kvstore_tests PRIVATE kvstore_lib GTest::gtest_main)
gtest_discover_tests(kvstore_tests)

# === Benchmarks ===
add_executable(kvstore_bench bench/bench_store.cpp)
target_link_libraries(kvstore_bench PRIVATE kvstore_lib benchmark::benchmark)

# === Sanitizer build ===
option(SANITIZER "Enable sanitizer (address, thread, undefined)" "")
if(SANITIZER)
    target_compile_options(kvstore_lib PUBLIC -fsanitize=${SANITIZER} -fno-omit-frame-pointer)
    target_link_options(kvstore_lib PUBLIC -fsanitize=${SANITIZER})
endif()
```

### vcpkg.json
```json
{
    "name": "kvstore",
    "version": "1.0.0",
    "dependencies": [
        "boost-asio",
        "boost-system",
        "gtest",
        "benchmark",
        "spdlog"
    ]
}
```

---

## Step 2: Common Types

### include/kvstore/common.hpp
```cpp
#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include <expected>

namespace kvstore {

using Clock = std::chrono::steady_clock;
using TimePoint = Clock::time_point;
using Duration = Clock::duration;

// Value stored for each key
struct Entry {
    std::string value;
    std::optional<TimePoint> expiry;  // nullopt = no expiration
    
    [[nodiscard]] bool isExpired() const {
        return expiry.has_value() && Clock::now() > *expiry;
    }
};

// Error types
enum class ErrorCode {
    Ok,
    KeyNotFound,
    InvalidCommand,
    InvalidArgument,
    IOError,
    ServerError,
};

// Result type for operations
template<typename T = void>
using Result = std::expected<T, ErrorCode>;

// Command types
enum class CommandType {
    GET, SET, DEL, KEYS, EXISTS, 
    EXPIRE, TTL,
    PING, QUIT, UNKNOWN
};

struct Command {
    CommandType type = CommandType::UNKNOWN;
    std::vector<std::string> args;
};

// WAL entry types
enum class WALOp : uint8_t {
    SET = 1,
    DEL = 2,
    EXPIRE = 3,
};

struct WALEntry {
    WALOp op;
    std::string key;
    std::string value;               // Empty for DEL
    int64_t ttlMs = 0;               // 0 = no TTL
    uint64_t sequenceNumber = 0;
};

} // namespace kvstore
```

---

## Step 3: Sharded Hash Map (Thread-Safe Core)

### include/kvstore/sharded_map.hpp
```cpp
#pragma once

#include "kvstore/common.hpp"
#include <array>
#include <functional>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>

namespace kvstore {

// Sharding strategy: hash the key, mod by shard count
// Each shard has its own mutex -> minimal contention
// With 64 shards and 8 threads: ~12.5% chance of contention per op

template<size_t NumShards = 64>
class ShardedMap {
public:
    // SET -> insert or update
    void set(const std::string& key, Entry entry) {
        auto& shard = getShard(key);
        std::unique_lock lock(shard.mutex);
        shard.data[key] = std::move(entry);
    }
    
    // GET -> returns copy of value (safe after lock release)
    std::optional<std::string> get(const std::string& key) {
        auto& shard = getShard(key);
        std::shared_lock lock(shard.mutex);  // Reader lock -> multiple concurrent reads
        
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
    
    // DEL -> returns true if key existed
    bool del(const std::string& key) {
        auto& shard = getShard(key);
        std::unique_lock lock(shard.mutex);
        return shard.data.erase(key) > 0;
    }
    
    // EXISTS -> check if key exists and not expired
    bool exists(const std::string& key) {
        auto& shard = getShard(key);
        std::shared_lock lock(shard.mutex);
        auto it = shard.data.find(key);
        if (it == shard.data.end()) return false;
        return !it->second.isExpired();
    }
    
    // EXPIRE -> set TTL on existing key
    bool expire(const std::string& key, Duration ttl) {
        auto& shard = getShard(key);
        std::unique_lock lock(shard.mutex);
        auto it = shard.data.find(key);
        if (it == shard.data.end()) return false;
        it->second.expiry = Clock::now() + ttl;
        return true;
    }
    
    // TTL -> get remaining time-to-live
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
    
    // KEYS -> return all non-expired keys matching pattern
    // WARNING: O(N) -> scans all shards, holds locks sequentially
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
    
    // Size -> approximate count (not atomic across shards)
    size_t size() const {
        size_t total = 0;
        for (const auto& shard : shards_) {
            std::shared_lock lock(shard.mutex);
            total += shard.data.size();
        }
        return total;
    }
    
    // Iterate all entries (for snapshots) -> caller must handle thread safety
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
```

---

## Step 4: Write-Ahead Log (WAL)

### include/kvstore/wal.hpp
```cpp
#pragma once

#include "kvstore/common.hpp"
#include <filesystem>
#include <fstream>
#include <mutex>
#include <vector>

namespace kvstore {

// WAL File Format:
// Each entry: [4-byte length][WALOp][key_len][key][val_len][value][8-byte ttl][8-byte seq]
// Append-only -> never modified, only appended or truncated after snapshot

class WriteAheadLog {
public:
    explicit WriteAheadLog(std::filesystem::path path)
        : path_(std::move(path)) {}
    
    // Open WAL file (create if not exists)
    Result<> open() {
        file_.open(path_, std::ios::binary | std::ios::app | std::ios::out);
        if (!file_.is_open()) {
            return std::unexpected(ErrorCode::IOError);
        }
        return {};
    }
    
    // Append a write operation to the WAL
    Result<> append(const WALEntry& entry) {
        std::lock_guard lock(mutex_);
        
        if (!file_.is_open()) {
            return std::unexpected(ErrorCode::IOError);
        }
        
        // Serialize entry
        auto data = serialize(entry);
        
        // Write length prefix + data
        uint32_t len = static_cast<uint32_t>(data.size());
        file_.write(reinterpret_cast<const char*>(&len), sizeof(len));
        file_.write(data.data(), static_cast<std::streamsize>(data.size()));
        file_.flush();  // Ensure durability (consider fdatasync for production)
        
        ++sequenceNumber_;
        return {};
    }
    
    // Read all entries from WAL (for recovery)
    Result<std::vector<WALEntry>> readAll() const {
        std::ifstream in(path_, std::ios::binary);
        if (!in.is_open()) {
            // No WAL file = fresh start
            return std::vector<WALEntry>{};
        }
        
        std::vector<WALEntry> entries;
        
        while (in.peek() != EOF) {
            uint32_t len = 0;
            in.read(reinterpret_cast<char*>(&len), sizeof(len));
            if (!in.good()) break;
            
            std::string data(len, '\0');
            in.read(data.data(), len);
            if (!in.good()) break;
            
            auto entry = deserialize(data);
            if (entry) {
                entries.push_back(std::move(*entry));
            }
        }
        
        return entries;
    }
    
    // Truncate WAL after a snapshot is taken
    Result<> truncate() {
        std::lock_guard lock(mutex_);
        file_.close();
        
        // Rename old WAL, create fresh one
        auto backup = path_;
        backup += ".bak";
        std::filesystem::rename(path_, backup);
        
        file_.open(path_, std::ios::binary | std::ios::app | std::ios::out);
        if (!file_.is_open()) {
            // Try to restore backup
            std::filesystem::rename(backup, path_);
            return std::unexpected(ErrorCode::IOError);
        }
        
        // Remove backup after successful truncation
        std::error_code ec;
        std::filesystem::remove(backup, ec);
        
        return {};
    }
    
    void close() {
        std::lock_guard lock(mutex_);
        if (file_.is_open()) {
            file_.flush();
            file_.close();
        }
    }
    
    uint64_t sequenceNumber() const { return sequenceNumber_; }
    
private:
    std::filesystem::path path_;
    std::ofstream file_;
    std::mutex mutex_;
    uint64_t sequenceNumber_ = 0;
    
    // Binary serialization
    static std::string serialize(const WALEntry& entry) {
        std::string data;
        
        // Op type
        data += static_cast<char>(entry.op);
        
        // Key (length-prefixed)
        uint32_t keyLen = static_cast<uint32_t>(entry.key.size());
        data.append(reinterpret_cast<const char*>(&keyLen), sizeof(keyLen));
        data.append(entry.key);
        
        // Value (length-prefixed)
        uint32_t valLen = static_cast<uint32_t>(entry.value.size());
        data.append(reinterpret_cast<const char*>(&valLen), sizeof(valLen));
        data.append(entry.value);
        
        // TTL
        data.append(reinterpret_cast<const char*>(&entry.ttlMs), sizeof(entry.ttlMs));
        
        // Sequence number
        data.append(reinterpret_cast<const char*>(&entry.sequenceNumber), sizeof(entry.sequenceNumber));
        
        return data;
    }
    
    static std::optional<WALEntry> deserialize(std::string_view data) {
        if (data.size() < 1 + 4 + 4 + 8 + 8) return std::nullopt;
        
        WALEntry entry;
        size_t pos = 0;
        
        // Op
        entry.op = static_cast<WALOp>(data[pos++]);
        
        // Key
        uint32_t keyLen;
        std::memcpy(&keyLen, data.data() + pos, sizeof(keyLen));
        pos += sizeof(keyLen);
        if (pos + keyLen > data.size()) return std::nullopt;
        entry.key = std::string(data.substr(pos, keyLen));
        pos += keyLen;
        
        // Value
        uint32_t valLen;
        std::memcpy(&valLen, data.data() + pos, sizeof(valLen));
        pos += sizeof(valLen);
        if (pos + valLen > data.size()) return std::nullopt;
        entry.value = std::string(data.substr(pos, valLen));
        pos += valLen;
        
        // TTL
        if (pos + sizeof(int64_t) > data.size()) return std::nullopt;
        std::memcpy(&entry.ttlMs, data.data() + pos, sizeof(entry.ttlMs));
        pos += sizeof(entry.ttlMs);
        
        // Sequence number
        if (pos + sizeof(uint64_t) > data.size()) return std::nullopt;
        std::memcpy(&entry.sequenceNumber, data.data() + pos, sizeof(entry.sequenceNumber));
        
        return entry;
    }
};

} // namespace kvstore
```

---

## Step 5: Snapshot Persistence

### include/kvstore/snapshot.hpp
```cpp
#pragma once

#include "kvstore/common.hpp"
#include "kvstore/sharded_map.hpp"
#include <filesystem>
#include <fstream>
#include <spdlog/spdlog.h>

namespace kvstore {

// Snapshot format: binary dump of all key-value pairs
// Used for fast startup (instead of replaying entire WAL)

class SnapshotManager {
public:
    explicit SnapshotManager(std::filesystem::path dir)
        : dir_(std::move(dir)) {
        std::filesystem::create_directories(dir_);
    }
    
    // Save current state to snapshot file
    template<size_t N>
    Result<> save(const ShardedMap<N>& map) {
        auto tempPath = dir_ / "snapshot.tmp";
        auto finalPath = dir_ / "snapshot.dat";
        
        {
            std::ofstream out(tempPath, std::ios::binary);
            if (!out.is_open()) {
                return std::unexpected(ErrorCode::IOError);
            }
            
            // Header: magic number + version
            constexpr uint32_t MAGIC = 0x4B565354;  // "KVST"
            constexpr uint32_t VERSION = 1;
            out.write(reinterpret_cast<const char*>(&MAGIC), sizeof(MAGIC));
            out.write(reinterpret_cast<const char*>(&VERSION), sizeof(VERSION));
            
            // Write all entries
            size_t count = 0;
            map.forEach([&](const std::string& key, const Entry& entry) {
                if (entry.isExpired()) return;  // Skip expired
                
                writeString(out, key);
                writeString(out, entry.value);
                
                // TTL: store as absolute epoch ms (or 0 for no expiry)
                int64_t expiryMs = 0;
                if (entry.expiry) {
                    auto epoch = entry.expiry->time_since_epoch();
                    expiryMs = std::chrono::duration_cast<std::chrono::milliseconds>(epoch).count();
                }
                out.write(reinterpret_cast<const char*>(&expiryMs), sizeof(expiryMs));
                ++count;
            });
            
            out.flush();
            if (!out.good()) {
                return std::unexpected(ErrorCode::IOError);
            }
            
            spdlog::info("Snapshot saved: {} entries", count);
        }
        
        // Atomic rename (crash-safe on most filesystems)
        std::error_code ec;
        std::filesystem::rename(tempPath, finalPath, ec);
        if (ec) {
            return std::unexpected(ErrorCode::IOError);
        }
        
        return {};
    }
    
    // Load snapshot into map
    template<size_t N>
    Result<size_t> load(ShardedMap<N>& map) {
        auto path = dir_ / "snapshot.dat";
        
        if (!std::filesystem::exists(path)) {
            spdlog::info("No snapshot found, starting fresh");
            return size_t{0};
        }
        
        std::ifstream in(path, std::ios::binary);
        if (!in.is_open()) {
            return std::unexpected(ErrorCode::IOError);
        }
        
        // Verify header
        uint32_t magic, version;
        in.read(reinterpret_cast<char*>(&magic), sizeof(magic));
        in.read(reinterpret_cast<char*>(&version), sizeof(version));
        
        if (magic != 0x4B565354 || version != 1) {
            spdlog::error("Invalid snapshot file");
            return std::unexpected(ErrorCode::IOError);
        }
        
        size_t count = 0;
        auto now = Clock::now();
        
        while (in.peek() != EOF) {
            auto key = readString(in);
            auto value = readString(in);
            if (!in.good()) break;
            
            int64_t expiryMs;
            in.read(reinterpret_cast<char*>(&expiryMs), sizeof(expiryMs));
            if (!in.good()) break;
            
            Entry entry;
            entry.value = std::move(value);
            
            if (expiryMs > 0) {
                auto epoch = std::chrono::milliseconds(expiryMs);
                auto expiry = TimePoint(std::chrono::duration_cast<Duration>(epoch));
                if (expiry <= now) continue;  // Already expired, skip
                entry.expiry = expiry;
            }
            
            map.set(key, std::move(entry));
            ++count;
        }
        
        spdlog::info("Snapshot loaded: {} entries", count);
        return count;
    }
    
private:
    std::filesystem::path dir_;
    
    static void writeString(std::ofstream& out, const std::string& s) {
        uint32_t len = static_cast<uint32_t>(s.size());
        out.write(reinterpret_cast<const char*>(&len), sizeof(len));
        out.write(s.data(), static_cast<std::streamsize>(len));
    }
    
    static std::string readString(std::ifstream& in) {
        uint32_t len;
        in.read(reinterpret_cast<char*>(&len), sizeof(len));
        std::string s(len, '\0');
        in.read(s.data(), len);
        return s;
    }
};

} // namespace kvstore
```

---

## Step 6: TTL Manager (Active + Lazy Expiration)

### include/kvstore/ttl_manager.hpp
```cpp
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
```

---

## Step 7: Protocol Parser

### include/kvstore/protocol.hpp
```cpp
#pragma once

#include "kvstore/common.hpp"
#include <string>
#include <string_view>
#include <charconv>

namespace kvstore {

// Text protocol (Redis-like, simplified):
// Request:  COMMAND arg1 arg2 ...\r\n
// Response: +OK\r\n            (success)
//           -ERR message\r\n   (error)
//           $5\r\nhello\r\n    (bulk string)
//           :42\r\n            (integer)
//           *0\r\n             (empty array)

class Protocol {
public:
    // Parse a command line into Command struct
    static Command parse(std::string_view line) {
        Command cmd;
        
        // Trim trailing \r\n
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) {
            line.remove_suffix(1);
        }
        
        if (line.empty()) {
            cmd.type = CommandType::UNKNOWN;
            return cmd;
        }
        
        // Split by spaces
        auto tokens = split(line);
        if (tokens.empty()) {
            cmd.type = CommandType::UNKNOWN;
            return cmd;
        }
        
        // Parse command type (case-insensitive)
        auto cmdStr = toUpper(tokens[0]);
        
        if (cmdStr == "GET")         cmd.type = CommandType::GET;
        else if (cmdStr == "SET")    cmd.type = CommandType::SET;
        else if (cmdStr == "DEL")    cmd.type = CommandType::DEL;
        else if (cmdStr == "KEYS")   cmd.type = CommandType::KEYS;
        else if (cmdStr == "EXISTS") cmd.type = CommandType::EXISTS;
        else if (cmdStr == "EXPIRE") cmd.type = CommandType::EXPIRE;
        else if (cmdStr == "TTL")    cmd.type = CommandType::TTL;
        else if (cmdStr == "PING")   cmd.type = CommandType::PING;
        else if (cmdStr == "QUIT")   cmd.type = CommandType::QUIT;
        else                         cmd.type = CommandType::UNKNOWN;
        
        // Remaining tokens are arguments
        for (size_t i = 1; i < tokens.size(); ++i) {
            cmd.args.emplace_back(tokens[i]);
        }
        
        return cmd;
    }
    
    // Format responses
    static std::string ok() { return "+OK\r\n"; }
    
    static std::string error(std::string_view msg) {
        return std::string("-ERR ") + std::string(msg) + "\r\n";
    }
    
    static std::string bulkString(std::string_view value) {
        return "$" + std::to_string(value.size()) + "\r\n" 
               + std::string(value) + "\r\n";
    }
    
    static std::string nullBulk() { return "$-1\r\n"; }
    
    static std::string integer(int64_t value) {
        return ":" + std::to_string(value) + "\r\n";
    }
    
    static std::string array(const std::vector<std::string>& items) {
        std::string result = "*" + std::to_string(items.size()) + "\r\n";
        for (const auto& item : items) {
            result += bulkString(item);
        }
        return result;
    }
    
    static std::string pong() { return "+PONG\r\n"; }
    
    // Parse integer from string
    static std::optional<int64_t> parseInt(std::string_view s) {
        int64_t value;
        auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), value);
        if (ec != std::errc{}) return std::nullopt;
        return value;
    }
    
private:
    static std::vector<std::string_view> split(std::string_view sv) {
        std::vector<std::string_view> tokens;
        size_t start = 0;
        
        while (start < sv.size()) {
            // Skip whitespace
            while (start < sv.size() && sv[start] == ' ') ++start;
            if (start >= sv.size()) break;
            
            // Handle quoted strings: SET key "hello world"
            if (sv[start] == '"') {
                ++start;
                auto end = sv.find('"', start);
                if (end == std::string_view::npos) end = sv.size();
                tokens.push_back(sv.substr(start, end - start));
                start = end + 1;
            } else {
                auto end = sv.find(' ', start);
                if (end == std::string_view::npos) end = sv.size();
                tokens.push_back(sv.substr(start, end - start));
                start = end + 1;
            }
        }
        
        return tokens;
    }
    
    static std::string toUpper(std::string_view sv) {
        std::string result(sv);
        for (char& c : result) {
            c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        }
        return result;
    }
};

} // namespace kvstore
```

---

## Step 8: KVStore Engine (Orchestrator)

### include/kvstore/store.hpp
```cpp
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
        return value -> Protocol::bulkString(*value) : Protocol::nullBulk();
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
        
        return Protocol::integer(ok -> 1 : 0);
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
```

---

## Step 9: TCP Server (Boost.Asio + Coroutines)

### include/kvstore/server.hpp
```cpp
#pragma once

#include "kvstore/store.hpp"
#include "kvstore/protocol.hpp"

#include <boost/asio.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/write.hpp>
#include <spdlog/spdlog.h>

namespace kvstore {

namespace asio = boost::asio;
using tcp = asio::ip::tcp;

class Server {
public:
    Server(KVStore& store, uint16_t port, unsigned threads = 0)
        : store_(store),
          port_(port),
          numThreads_(threads > 0 ? threads : std::thread::hardware_concurrency()),
          io_(static_cast<int>(numThreads_)) {}
    
    // Start the server (blocking -> runs the event loop)
    void run() {
        tcp::acceptor acceptor(io_, {tcp::v4(), port_});
        acceptor.set_option(tcp::acceptor::reuse_address(true));
        
        spdlog::info("KVStore server listening on port {} ({} threads)", 
                     port_, numThreads_);
        
        asio::co_spawn(io_, accept(std::move(acceptor)), asio::detached);
        
        // Run on multiple threads
        std::vector<std::jthread> threads;
        for (unsigned i = 1; i < numThreads_; ++i) {
            threads.emplace_back([this] { io_.run(); });
        }
        io_.run();  // Main thread
    }
    
    void stop() {
        io_.stop();
        spdlog::info("Server stopped");
    }
    
private:
    KVStore& store_;
    uint16_t port_;
    unsigned numThreads_;
    asio::io_context io_;
    
    // Accept loop coroutine
    asio::awaitable<void> accept(tcp::acceptor acceptor) {
        for (;;) {
            auto socket = co_await acceptor.async_accept(asio::use_awaitable);
            auto endpoint = socket.remote_endpoint();
            spdlog::debug("Client connected: {}:{}", 
                         endpoint.address().to_string(), endpoint.port());
            
            auto executor = co_await asio::this_coro::executor;
            asio::co_spawn(executor, 
                           handleClient(std::move(socket)), 
                           asio::detached);
        }
    }
    
    // Per-client connection handler
    asio::awaitable<void> handleClient(tcp::socket socket) {
        try {
            asio::streambuf buf;
            
            for (;;) {
                // Read one line (command)
                auto bytes = co_await asio::async_read_until(
                    socket, buf, "\n", asio::use_awaitable);
                
                // Extract the line
                std::istream stream(&buf);
                std::string line;
                std::getline(stream, line);
                
                // Parse and execute
                auto cmd = Protocol::parse(line);
                
                if (cmd.type == CommandType::QUIT) {
                    auto response = Protocol::ok();
                    co_await asio::async_write(
                        socket, asio::buffer(response), asio::use_awaitable);
                    break;
                }
                
                auto response = store_.execute(cmd);
                
                co_await asio::async_write(
                    socket, asio::buffer(response), asio::use_awaitable);
            }
        } catch (const boost::system::system_error& e) {
            if (e.code() != asio::error::eof && 
                e.code() != asio::error::connection_reset) {
                spdlog::warn("Client error: {}", e.what());
            }
        }
        spdlog::debug("Client disconnected");
    }
};

} // namespace kvstore
```

---

## Step 10: Main Entry Point

### src/main.cpp
```cpp
#include "kvstore/store.hpp"
#include "kvstore/server.hpp"
#include <spdlog/spdlog.h>
#include <csignal>
#include <iostream>

using namespace kvstore;

static std::atomic<bool> g_running{true};

void signalHandler(int signum) {
    spdlog::info("Received signal {}, shutting down...", signum);
    g_running = false;
}

int main(int argc, char* argv[]) {
    // Parse command line
    uint16_t port = 6379;       // Default Redis port
    std::string dataDir = "./kvdata";
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "--port" || arg == "-p") && i + 1 < argc) {
            port = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if ((arg == "--dir" || arg == "-d") && i + 1 < argc) {
            dataDir = argv[++i];
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: kvstore [options]\n"
                      << "  -p, --port PORT   Listen port (default: 6379)\n"
                      << "  -d, --dir  DIR    Data directory (default: ./kvdata)\n"
                      << "  -h, --help        Show this help\n";
            return 0;
        }
    }
    
    // Setup logging
    spdlog::set_level(spdlog::level::info);
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%t] %v");
    
    // Signal handling
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    
    // Create and open store
    StoreConfig config{
        .dataDir = dataDir,
        .ttlScanInterval = std::chrono::seconds(1),
        .snapshotInterval = std::chrono::minutes(5),
        .enablePersistence = true,
    };
    
    KVStore store(config);
    auto result = store.open();
    if (!result) {
        spdlog::error("Failed to open store: {}", static_cast<int>(result.error()));
        return 1;
    }
    
    // Run server
    Server server(store, port);
    
    spdlog::info("Starting KVStore server on port {}...", port);
    spdlog::info("Data directory: {}", dataDir);
    spdlog::info("Use: redis-cli -p {} to connect", port);
    
    server.run();  // Blocks until stopped
    
    store.close();
    return 0;
}
```

---

## Step 11: Unit Tests

### tests/test_store.cpp
```cpp
#include "kvstore/store.hpp"
#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <filesystem>

using namespace kvstore;

class StoreTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Use temp directory for each test
        testDir_ = std::filesystem::temp_directory_path() / "kvstore_test";
        std::filesystem::remove_all(testDir_);
        
        StoreConfig config{
            .dataDir = testDir_,
            .ttlScanInterval = std::chrono::milliseconds(100),
            .snapshotInterval = std::chrono::hours(1),  // Don't auto-snapshot in tests
            .enablePersistence = true,
        };
        store_ = std::make_unique<KVStore>(config);
        auto result = store_->open();
        ASSERT_TRUE(result.has_value());
    }
    
    void TearDown() override {
        store_->close();
        store_.reset();
        std::filesystem::remove_all(testDir_);
    }
    
    std::filesystem::path testDir_;
    std::unique_ptr<KVStore> store_;
};

TEST_F(StoreTest, SetAndGet) {
    store_->set("name", "Alice");
    auto val = store_->get("name");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, "Alice");
}

TEST_F(StoreTest, GetNonExistent) {
    auto val = store_->get("missing");
    EXPECT_FALSE(val.has_value());
}

TEST_F(StoreTest, Delete) {
    store_->set("key", "value");
    EXPECT_TRUE(store_->del("key"));
    EXPECT_FALSE(store_->get("key").has_value());
}

TEST_F(StoreTest, DeleteNonExistent) {
    EXPECT_FALSE(store_->del("missing"));
}

TEST_F(StoreTest, Overwrite) {
    store_->set("key", "v1");
    store_->set("key", "v2");
    EXPECT_EQ(*store_->get("key"), "v2");
}

TEST_F(StoreTest, TTLExpiration) {
    store_->set("temp", "data", std::chrono::milliseconds(50));
    EXPECT_TRUE(store_->get("temp").has_value());
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_FALSE(store_->get("temp").has_value());
}

TEST_F(StoreTest, PersistenceRecovery) {
    // Write data
    store_->set("persistent", "data");
    store_->set("also", "here");
    store_->close();
    
    // Reopen store -> should recover from WAL
    StoreConfig config{
        .dataDir = testDir_,
        .enablePersistence = true,
    };
    auto store2 = std::make_unique<KVStore>(config);
    auto result = store2->open();
    ASSERT_TRUE(result.has_value());
    
    EXPECT_EQ(*store2->get("persistent"), "data");
    EXPECT_EQ(*store2->get("also"), "here");
    
    store2->close();
}

TEST_F(StoreTest, ConcurrentAccess) {
    constexpr int NUM_THREADS = 8;
    constexpr int OPS_PER_THREAD = 10000;
    
    std::vector<std::jthread> threads;
    std::atomic<int> errors{0};
    
    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([this, t, &errors] {
            for (int i = 0; i < OPS_PER_THREAD; ++i) {
                std::string key = "key_" + std::to_string(t) + "_" + std::to_string(i);
                std::string val = "val_" + std::to_string(i);
                
                store_->set(key, val);
                auto result = store_->get(key);
                
                if (!result || *result != val) {
                    ++errors;
                }
            }
        });
    }
    
    threads.clear();  // Join all
    EXPECT_EQ(errors.load(), 0);
    EXPECT_EQ(store_->size(), NUM_THREADS * OPS_PER_THREAD);
}
```

### tests/test_protocol.cpp
```cpp
#include "kvstore/protocol.hpp"
#include <gtest/gtest.h>

using namespace kvstore;

TEST(ProtocolTest, ParseGet) {
    auto cmd = Protocol::parse("GET mykey\r\n");
    EXPECT_EQ(cmd.type, CommandType::GET);
    ASSERT_EQ(cmd.args.size(), 1);
    EXPECT_EQ(cmd.args[0], "mykey");
}

TEST(ProtocolTest, ParseSetWithExpiry) {
    auto cmd = Protocol::parse("SET mykey myvalue EX 60\r\n");
    EXPECT_EQ(cmd.type, CommandType::SET);
    ASSERT_EQ(cmd.args.size(), 4);
    EXPECT_EQ(cmd.args[0], "mykey");
    EXPECT_EQ(cmd.args[1], "myvalue");
    EXPECT_EQ(cmd.args[2], "EX");
    EXPECT_EQ(cmd.args[3], "60");
}

TEST(ProtocolTest, ParseCaseInsensitive) {
    auto cmd = Protocol::parse("get KEY\r\n");
    EXPECT_EQ(cmd.type, CommandType::GET);
}

TEST(ProtocolTest, ParseQuotedString) {
    auto cmd = Protocol::parse("SET key \"hello world\"\r\n");
    EXPECT_EQ(cmd.type, CommandType::SET);
    ASSERT_EQ(cmd.args.size(), 2);
    EXPECT_EQ(cmd.args[1], "hello world");
}

TEST(ProtocolTest, ParseEmpty) {
    auto cmd = Protocol::parse("\r\n");
    EXPECT_EQ(cmd.type, CommandType::UNKNOWN);
}

TEST(ProtocolTest, ParseDel) {
    auto cmd = Protocol::parse("DEL k1 k2 k3\r\n");
    EXPECT_EQ(cmd.type, CommandType::DEL);
    EXPECT_EQ(cmd.args.size(), 3);
}

TEST(ProtocolTest, FormatBulkString) {
    EXPECT_EQ(Protocol::bulkString("hello"), "$5\r\nhello\r\n");
}

TEST(ProtocolTest, FormatNull) {
    EXPECT_EQ(Protocol::nullBulk(), "$-1\r\n");
}

TEST(ProtocolTest, FormatInteger) {
    EXPECT_EQ(Protocol::integer(42), ":42\r\n");
}

TEST(ProtocolTest, FormatArray) {
    auto result = Protocol::array({"a", "b", "c"});
    EXPECT_EQ(result, "*3\r\n$1\r\na\r\n$1\r\nb\r\n$1\r\nc\r\n");
}
```

### tests/test_sharded_map.cpp
```cpp
#include "kvstore/sharded_map.hpp"
#include <gtest/gtest.h>
#include <thread>

using namespace kvstore;

TEST(ShardedMapTest, BasicOperations) {
    ShardedMap<16> map;
    
    map.set("hello", Entry{.value = "world"});
    auto val = map.get("hello");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, "world");
    
    EXPECT_TRUE(map.exists("hello"));
    EXPECT_FALSE(map.exists("missing"));
    
    EXPECT_TRUE(map.del("hello"));
    EXPECT_FALSE(map.get("hello").has_value());
}

TEST(ShardedMapTest, TTLExpiry) {
    ShardedMap<16> map;
    
    Entry entry{
        .value = "temporary",
        .expiry = Clock::now() + std::chrono::milliseconds(50)
    };
    map.set("temp", std::move(entry));
    
    EXPECT_TRUE(map.get("temp").has_value());
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_FALSE(map.get("temp").has_value());  // Lazy expiration
}

TEST(ShardedMapTest, PurgeExpired) {
    ShardedMap<16> map;
    
    auto pastExpiry = Clock::now() - std::chrono::seconds(1);
    map.set("expired1", Entry{.value = "a", .expiry = pastExpiry});
    map.set("expired2", Entry{.value = "b", .expiry = pastExpiry});
    map.set("alive", Entry{.value = "c"});
    
    auto purged = map.purgeExpired();
    EXPECT_EQ(purged, 2);
    EXPECT_EQ(map.size(), 1);
    EXPECT_TRUE(map.get("alive").has_value());
}

TEST(ShardedMapTest, KeysPattern) {
    ShardedMap<16> map;
    
    map.set("user:alice", Entry{.value = "1"});
    map.set("user:bob", Entry{.value = "2"});
    map.set("order:123", Entry{.value = "3"});
    
    auto allKeys = map.keys("*");
    EXPECT_EQ(allKeys.size(), 3);
    
    auto userKeys = map.keys("user:*");
    EXPECT_EQ(userKeys.size(), 2);
}

TEST(ShardedMapTest, ConcurrentReadWrite) {
    ShardedMap<32> map;
    constexpr int N = 50000;
    
    // Writer thread
    std::jthread writer([&map] {
        for (int i = 0; i < N; ++i) {
            map.set("key" + std::to_string(i), Entry{.value = std::to_string(i)});
        }
    });
    
    // Reader thread
    std::atomic<int> found{0};
    std::jthread reader([&map, &found] {
        for (int i = 0; i < N; ++i) {
            if (map.get("key" + std::to_string(i)).has_value()) {
                ++found;
            }
        }
    });
    
    writer.join();
    reader.join();
    
    // All keys should exist after writer is done
    EXPECT_EQ(map.size(), N);
}
```

### tests/test_wal.cpp
```cpp
#include "kvstore/wal.hpp"
#include <gtest/gtest.h>
#include <filesystem>

using namespace kvstore;

class WALTest : public ::testing::Test {
protected:
    void SetUp() override {
        testPath_ = std::filesystem::temp_directory_path() / "test_wal.log";
        std::filesystem::remove(testPath_);
    }
    
    void TearDown() override {
        std::filesystem::remove(testPath_);
    }
    
    std::filesystem::path testPath_;
};

TEST_F(WALTest, AppendAndRead) {
    {
        WriteAheadLog wal(testPath_);
        ASSERT_TRUE(wal.open().has_value());
        
        wal.append({.op = WALOp::SET, .key = "k1", .value = "v1"});
        wal.append({.op = WALOp::SET, .key = "k2", .value = "v2", .ttlMs = 5000});
        wal.append({.op = WALOp::DEL, .key = "k1"});
        wal.close();
    }
    
    {
        WriteAheadLog wal(testPath_);
        auto entries = wal.readAll();
        ASSERT_TRUE(entries.has_value());
        ASSERT_EQ(entries->size(), 3);
        
        EXPECT_EQ((*entries)[0].op, WALOp::SET);
        EXPECT_EQ((*entries)[0].key, "k1");
        EXPECT_EQ((*entries)[0].value, "v1");
        
        EXPECT_EQ((*entries)[1].ttlMs, 5000);
        
        EXPECT_EQ((*entries)[2].op, WALOp::DEL);
        EXPECT_EQ((*entries)[2].key, "k1");
    }
}

TEST_F(WALTest, EmptyWAL) {
    WriteAheadLog wal(testPath_);
    auto entries = wal.readAll();
    ASSERT_TRUE(entries.has_value());
    EXPECT_TRUE(entries->empty());
}

TEST_F(WALTest, Truncate) {
    WriteAheadLog wal(testPath_);
    ASSERT_TRUE(wal.open().has_value());
    
    wal.append({.op = WALOp::SET, .key = "k1", .value = "v1"});
    wal.append({.op = WALOp::SET, .key = "k2", .value = "v2"});
    
    ASSERT_TRUE(wal.truncate().has_value());
    
    auto entries = wal.readAll();
    ASSERT_TRUE(entries.has_value());
    EXPECT_TRUE(entries->empty());
    
    wal.close();
}

TEST_F(WALTest, LargeValues) {
    WriteAheadLog wal(testPath_);
    ASSERT_TRUE(wal.open().has_value());
    
    std::string bigValue(1024 * 1024, 'X');  // 1MB value
    wal.append({.op = WALOp::SET, .key = "big", .value = bigValue});
    wal.close();
    
    auto entries = wal.readAll();
    ASSERT_TRUE(entries.has_value());
    ASSERT_EQ(entries->size(), 1);
    EXPECT_EQ((*entries)[0].value.size(), 1024 * 1024);
}
```

### tests/test_integration.cpp
```cpp
#include "kvstore/store.hpp"
#include "kvstore/protocol.hpp"
#include <gtest/gtest.h>
#include <filesystem>

using namespace kvstore;

class IntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        testDir_ = std::filesystem::temp_directory_path() / "kvstore_integration";
        std::filesystem::remove_all(testDir_);
        
        StoreConfig config{
            .dataDir = testDir_,
            .ttlScanInterval = std::chrono::milliseconds(50),
            .enablePersistence = true,
        };
        store_ = std::make_unique<KVStore>(config);
        ASSERT_TRUE(store_->open().has_value());
    }
    
    void TearDown() override {
        store_->close();
        store_.reset();
        std::filesystem::remove_all(testDir_);
    }
    
    std::string exec(std::string_view cmdLine) {
        auto cmd = Protocol::parse(cmdLine);
        return store_->execute(cmd);
    }
    
    std::filesystem::path testDir_;
    std::unique_ptr<KVStore> store_;
};

TEST_F(IntegrationTest, EndToEndWorkflow) {
    // SET
    EXPECT_EQ(exec("SET user:1 Alice"), "+OK\r\n");
    EXPECT_EQ(exec("SET user:2 Bob"), "+OK\r\n");
    
    // GET
    EXPECT_EQ(exec("GET user:1"), "$5\r\nAlice\r\n");
    EXPECT_EQ(exec("GET user:2"), "$3\r\nBob\r\n");
    EXPECT_EQ(exec("GET user:3"), "$-1\r\n");
    
    // EXISTS
    EXPECT_EQ(exec("EXISTS user:1"), ":1\r\n");
    EXPECT_EQ(exec("EXISTS user:99"), ":0\r\n");
    
    // DEL
    EXPECT_EQ(exec("DEL user:1"), ":1\r\n");
    EXPECT_EQ(exec("GET user:1"), "$-1\r\n");
    
    // DEL multiple
    EXPECT_EQ(exec("SET a 1"), "+OK\r\n");
    EXPECT_EQ(exec("SET b 2"), "+OK\r\n");
    EXPECT_EQ(exec("DEL a b c"), ":2\r\n");  // c doesn't exist
    
    // PING
    EXPECT_EQ(exec("PING"), "+PONG\r\n");
    
    // Unknown command
    auto response = exec("INVALID");
    EXPECT_TRUE(response.starts_with("-ERR"));
}

TEST_F(IntegrationTest, SetWithExpiry) {
    EXPECT_EQ(exec("SET temp data EX 1"), "+OK\r\n");
    EXPECT_EQ(exec("GET temp"), "$4\r\ndata\r\n");
    
    // Check TTL
    auto ttlResponse = exec("TTL temp");
    // Should be :0 or :1
    EXPECT_TRUE(ttlResponse == ":0\r\n" || ttlResponse == ":1\r\n");
    
    // Wait for expiry
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    EXPECT_EQ(exec("GET temp"), "$-1\r\n");
}

TEST_F(IntegrationTest, ExpireCommand) {
    EXPECT_EQ(exec("SET key value"), "+OK\r\n");
    EXPECT_EQ(exec("TTL key"), ":-1\r\n");  // No expiry
    
    EXPECT_EQ(exec("EXPIRE key 10"), ":1\r\n");
    
    auto ttlResponse = exec("TTL key");
    // Should be between :8 and :10
    EXPECT_TRUE(ttlResponse.starts_with(":"));
    
    // EXPIRE non-existent key
    EXPECT_EQ(exec("EXPIRE missing 10"), ":0\r\n");
    
    // TTL non-existent key
    EXPECT_EQ(exec("TTL missing"), ":-2\r\n");
}

TEST_F(IntegrationTest, ErrorHandling) {
    EXPECT_TRUE(exec("GET").starts_with("-ERR"));           // Missing arg
    EXPECT_TRUE(exec("SET key").starts_with("-ERR"));       // Missing value
    EXPECT_TRUE(exec("SET k v EX -5").starts_with("-ERR")); // Negative TTL
    EXPECT_TRUE(exec("EXPIRE k abc").starts_with("-ERR"));  // Non-numeric TTL
}
```

---

## Step 12: Benchmarks

### bench/bench_store.cpp
```cpp
#include "kvstore/store.hpp"
#include <benchmark/benchmark.h>
#include <filesystem>
#include <random>

using namespace kvstore;

class StoreFixture : public benchmark::Fixture {
public:
    void SetUp(benchmark::State&) override {
        testDir_ = std::filesystem::temp_directory_path() / "kvstore_bench";
        std::filesystem::remove_all(testDir_);
        
        StoreConfig config{
            .dataDir = testDir_,
            .enablePersistence = false,  // Benchmark without I/O
        };
        store_ = std::make_unique<KVStore>(config);
        store_->open();
    }
    
    void TearDown(benchmark::State&) override {
        store_->close();
        store_.reset();
        std::filesystem::remove_all(testDir_);
    }
    
    std::filesystem::path testDir_;
    std::unique_ptr<KVStore> store_;
};

BENCHMARK_DEFINE_F(StoreFixture, SetSequential)(benchmark::State& state) {
    int i = 0;
    for (auto _ : state) {
        store_->set("key" + std::to_string(i++), "value");
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK_REGISTER_F(StoreFixture, SetSequential)->Threads(1)->Threads(4)->Threads(8);

BENCHMARK_DEFINE_F(StoreFixture, GetExisting)(benchmark::State& state) {
    // Pre-populate
    for (int i = 0; i < 100000; ++i) {
        store_->set("key" + std::to_string(i), "value" + std::to_string(i));
    }
    
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> dist(0, 99999);
    
    for (auto _ : state) {
        auto key = "key" + std::to_string(dist(rng));
        benchmark::DoNotOptimize(store_->get(key));
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK_REGISTER_F(StoreFixture, GetExisting)->Threads(1)->Threads(4)->Threads(8);

BENCHMARK_DEFINE_F(StoreFixture, GetMiss)(benchmark::State& state) {
    for (auto _ : state) {
        benchmark::DoNotOptimize(store_->get("nonexistent"));
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK_REGISTER_F(StoreFixture, GetMiss);

BENCHMARK_DEFINE_F(StoreFixture, MixedWorkload)(benchmark::State& state) {
    // 80% reads, 20% writes (typical cache workload)
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> opDist(0, 99);
    std::uniform_int_distribution<int> keyDist(0, 9999);
    
    // Pre-populate
    for (int i = 0; i < 10000; ++i) {
        store_->set("key" + std::to_string(i), "value");
    }
    
    for (auto _ : state) {
        auto key = "key" + std::to_string(keyDist(rng));
        if (opDist(rng) < 80) {
            benchmark::DoNotOptimize(store_->get(key));
        } else {
            store_->set(key, "newvalue");
        }
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK_REGISTER_F(StoreFixture, MixedWorkload)->Threads(1)->Threads(4)->Threads(8);

BENCHMARK_MAIN();
```

---

# ENHANCED SECTION: Interview Discussion Points for KVStore

> *When presenting this project, these are the follow-up questions a senior interviewer will ask. Prepare answers for each.*

---

## Discussion Q1: "How would you scale this to a distributed KVStore?"

### Answer:
```
Single Node -> Distributed:

1. PARTITIONING (Sharding)
   - Consistent hashing -> minimal reshuffling when nodes join/leave
   - Each node owns a range of hash space
   - Client library routes requests to correct shard
   
2. REPLICATION
   - Each partition replicated to N nodes (typically 3)
   - Leader-follower: writes go to leader, reads from any replica
   - This is EXACTLY what iCluster does -> leader (source) replicates to followers (targets)
   
3. CONSENSUS
   - Raft for leader election and log replication
   - Quorum writes: W + R > N for strong consistency
   
4. FAILURE HANDLING
   - Node failure: Promote a follower to leader (iCluster DMSETPRIM)
   - Network partition: CP (reject writes) or AP (accept writes, reconcile later)
   - Recovery: Full resync from leader (iCluster DMRFSHOBJ refresh)

Architecture:
+-----------+    +-----------+    +-----------+
|  Shard 0  |    |  Shard 1  |    |  Shard 2  |
|  Leader   ->    |  Leader   ->    |  Leader   |
|  keys 0-33|    |  keys 34-66|   |  keys 67-99|
+-----------+    +-----------+    +-----------+
      -                ?                ?
+-----------+    +-----------+    +-----------+
|  Replica  ->    |  Replica  ->    |  Replica  |
|  (Follower)|   |  (Follower)|   |  (Follower)|
+-----------+    +-----------+    +-----------+
```

---

## Discussion Q2: "What would you change for a production deployment?"

### Answer:
```
1. PERSISTENCE
   - Replace file-based WAL with memory-mapped WAL (mmap + msync)
   - Use RocksDB or LevelDB as storage engine instead of custom
   - Implement compaction for WAL (merge snapshots + logs)

2. NETWORKING
   - Binary protocol instead of text (MessagePack, FlatBuffers)
   - Connection pooling and multiplexing
   - TLS for encryption
   
3. OPERATIONS
   - Prometheus metrics endpoint (/metrics)
   - Health check endpoint (/health)
   - Graceful shutdown with drain period
   - Configuration via YAML/TOML file
   
4. RELIABILITY
   - Replication to standby (like iCluster's source-target model)
   - Automatic failover with heartbeat detection
   - Backup/restore capability
   
5. PERFORMANCE
   - Custom allocator for small string keys (pool allocator from Set 7)
   - SIMD-accelerated string comparison for hot keys
   - Adaptive shard count based on load
```

---

## Discussion Q3: "How does your KVStore compare to Redis architecture?"

### Answer:
| Feature | Our KVStore | Redis |
|---------|------------|-------|
| Threading | Multi-threaded (sharded) | Single-threaded event loop (7.0+ adds I/O threads) |
| Persistence | WAL + snapshots | RDB snapshots + AOF (append-only file) |
| TTL | Lazy + active eviction | Lazy + probabilistic active expiry |
| Protocol | Text-based | RESP (Redis Serialization Protocol) |
| Data types | String only | String, List, Set, Hash, Sorted Set, Stream |
| Replication | Not implemented | Master-replica with async replication |

**Key insight**: Redis's single-threaded model simplifies reasoning about concurrency but limits CPU utilization. Our sharded approach uses all cores. Redis 7.0+ adds I/O threads for network handling while keeping command execution single-threaded | a hybrid approach.

**iCluster comparison**: iCluster replicates at the object level (entire files/tables), while Redis/our KVStore replicate at the key level. iCluster's journal-based CDC is analogous to Redis's AOF replication | both are append-only log shipping.

---

## Step 13: Usage & Demo

### Building and Running
```bash
# Build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# Run tests
ctest --test-dir build --output-on-failure

# Run benchmarks
./build/kvstore_bench

# Run server
./build/kvstore --port 6379 --dir ./mydata

# Connect with redis-cli (or telnet/netcat)
redis-cli -p 6379

> SET greeting "Hello, World!"
OK
> GET greeting
"Hello, World!"
> SET session:abc token123 EX 3600
OK
> TTL session:abc
(integer) 3599
> KEYS session:*
1) "session:abc"
> DEL greeting
(integer) 1
> PING
PONG
> QUIT
OK
```

### With Sanitizers
```bash
# Address Sanitizer (memory bugs)
cmake -B build-asan -DSANITIZER=address
cmake --build build-asan
ctest --test-dir build-asan

# Thread Sanitizer (race conditions)
cmake -B build-tsan -DSANITIZER=thread
cmake --build build-tsan
ctest --test-dir build-tsan

# Undefined Behavior Sanitizer
cmake -B build-ubsan -DSANITIZER=undefined
cmake --build build-ubsan
ctest --test-dir build-ubsan
```

---

## Architecture Decisions & Trade-offs (Interview Discussion Points)

### Why Sharded Map instead of a single mutex|
```
Single mutex:        100K ops/sec (all threads serialize)
Sharded (64 shards): 2M+ ops/sec (12.5% contention with 8 threads)
Lock-free:           5M+ ops/sec (but much more complex code)

We chose sharding because:
  - Simple to implement and reason about
  - Well-understood performance characteristics
  - shared_mutex allows concurrent reads
  - Not optimal for very high write ratios
  - Not optimal for hotspot keys (same shard)
```

### Why WAL + Snapshot instead of just WAL|
```
WAL only:
  - Recovery replays ALL operations since dawn of time
  - WAL file grows forever -> disk exhaustion
  - Recovery time: O(total operations ever)

WAL + Snapshot:
  - Snapshot = point-in-time full dump
  - WAL only stores operations AFTER last snapshot
  - Recovery: Load snapshot + replay recent WAL
  - Recovery time: O(operations since last snapshot)
  
This is the same design as:
  - PostgreSQL (WAL + base backups)
  - Redis (AOF + RDB snapshots)
  - SQLite (WAL + checkpointing)
```

### Why text protocol instead of binary|
```
Text protocol:
  - Debuggable with telnet/netcat
  - Compatible with redis-cli
  - Easy to implement and test
  - Parsing overhead
  - Larger wire size

Binary protocol (e.g., FlatBuffers/Protobuf):
  - Faster parsing
  - Smaller wire size
  - Need custom client
  - Not debuggable with standard tools

For an interview project, text is better (demonstrates more,
debuggable, compatible with existing tools).
```

### What would a production version add|
```
Must-have:
  - Authentication (AUTH command)
  - Access control (read-only vs read-write)
  - TLS encryption for network traffic
  - Connection limits and rate limiting
  - Memory limits with eviction policies (LRU, LFU, random)
  - Replication (leader-follower)
  - Cluster mode (hash slots, like Redis Cluster)
  - Pub/Sub channels
  - Transactions (MULTI/EXEC)
  - Lua scripting engine
  
Nice-to-have:
  - Data types beyond strings (lists, sets, sorted sets, hashes)
  - Pipelining (batch commands)
  - Client-side caching
  - Streams (append-only log data type)
  - Geo commands (geospatial indexing)
```

### Complexity Analysis
```
+----------------------------------------------------------------+
| Operation| Time     -> Notes                                    |
+----------------------------------------------------------------+
| GET      -> O(1) avg -> Hash lookup + shared lock                |
| SET      -> O(1) avg -> Hash insert + unique lock + WAL append   |
| DEL      -> O(1) avg -> Hash erase + unique lock + WAL append    |
| EXISTS   -> O(1) avg -> Hash lookup + shared lock                |
| KEYS     -> O(N)     | Full scan -> blocks shard by shard        |
| TTL      -> O(1)     | Single lookup                            |
| EXPIRE   -> O(1)     | Single lookup + update                   |
| PURGE    -> O(N)     | Full scan -> done in background thread    |
| SNAPSHOT -> O(N)     | Full scan -> done in background thread    |
+----------------------------------------------------------------+
Space: O(N) where N = number of entries
WAL: O(M) where M = operations since last snapshot
```

---

## Summary: What This Project Demonstrates

```
- Modern C++ (C++20)      | concepts, jthread, expected, designated initializers
- Concurrency              -> sharded locking, shared_mutex, atomics, jthread
- Networking               -> Boost.Asio, async TCP, coroutines
- Persistence              -> WAL, snapshots, crash recovery
- Data Structures           -> hash maps, sharding
- Memory Management         -> RAII, smart pointers, no raw new/delete
- Error Handling            -> std::expected, proper error propagation
- Testing                   -> unit, integration, concurrency, benchmarks
- Build System              -> CMake, vcpkg, sanitizer support
- API Design                -> clean public interface, protocol design
- Logging                   -> structured logging with spdlog
- Signal Handling           -> graceful shutdown
- File I/O                  -> binary serialization, atomic rename
- Design Patterns           -> RAII, Strategy (eviction), Observer (TTL)
- Production Thinking       -> what would you add? (security, replication, etc.)
```

> **Interview tip**: When presenting this project, walk through the architecture diagram first, then drill into 2-3 interesting components (sharded map, WAL, coroutine server). Be ready to discuss trade-offs and what you'd change for production.

---

## Step 12: Production Readiness Discussion — Deployment, Observability, and HA

### Architecture Discussion Questions

**Q1: How would you deploy this KVStore in production?**
```
SINGLE NODE:
  - Docker container with persistent volume for WAL + snapshots
  - Systemd service with auto-restart
  - Health check endpoint (TCP ping or HTTP /health)
  - Graceful shutdown on SIGTERM (flush WAL, complete in-flight ops)

REPLICATED (HA):
  - Leader-follower topology (leader handles writes, followers read)
  - WAL entries replicated to followers before acknowledging to client
  - Raft consensus for leader election (use existing library: nuraft, braft)
  
  +----------+     WAL stream     +----------+
  | Leader   | ─────────────────→ | Follower | 
  | (R/W)    |                    | (R only) |
  +----------+     WAL stream     +----------+
       |          ─────────────→ | Follower |
       |                          +----------+
       ↓
  Client writes go here
  Client reads can go to any node (eventual consistency)
  
  Consistency options:
    - Strong: Read from leader only
    - Eventual: Read from any (may see stale data)
    - Bounded staleness: Read from follower within N ms of leader

SHARDED (Horizontal Scale):
  - Consistent hashing to distribute keys across N nodes
  - Each shard is independently replicated
  - Proxy/router layer maps key → shard
  - Rebalancing: when adding/removing nodes, migrate key ranges
```

**Q2: How would you add observability?**
```cpp
// Metrics to expose (Prometheus-compatible):
struct KVStoreMetrics {
    // Counters
    std::atomic<uint64_t> total_gets{0};
    std::atomic<uint64_t> total_sets{0};
    std::atomic<uint64_t> total_deletes{0};
    std::atomic<uint64_t> cache_hits{0};
    std::atomic<uint64_t> cache_misses{0};
    std::atomic<uint64_t> wal_writes{0};
    std::atomic<uint64_t> wal_bytes_written{0};
    std::atomic<uint64_t> expired_keys{0};
    std::atomic<uint64_t> evicted_keys{0};
    
    // Gauges
    std::atomic<uint64_t> current_keys{0};
    std::atomic<uint64_t> memory_bytes{0};
    std::atomic<uint64_t> connected_clients{0};
    std::atomic<uint64_t> wal_size_bytes{0};
    
    // Histograms (latency distribution)
    // P50, P95, P99 for GET, SET, DELETE operations
    // WAL sync latency
    // Snapshot duration
};

// Structured log example:
// {"ts":"2024-01-15T10:30:45.123Z","level":"info","msg":"snapshot_complete",
//  "keys":1234567,"size_mb":256,"duration_ms":1500,"wal_entries_compacted":50000}

// Health check endpoint:
// GET /health → {"status":"healthy","uptime_seconds":86400,
//                "keys":1234567,"memory_mb":512,"wal_lag_entries":0}
```

**Q3: What about security?**
```
1. AUTHENTICATION
   - AUTH command (like Redis) — simple password
   - TLS client certificates for production
   
2. AUTHORIZATION (ACL)
   - Per-key-prefix permissions: user:alice → read/write keys "alice:*"
   - Admin role for SNAPSHOT, CONFIG commands
   
3. NETWORK SECURITY
   - TLS for all connections (Boost.Asio + OpenSSL)
   - Bind to specific interface (not 0.0.0.0 in production)
   - Rate limiting per client IP
   
4. DATA SECURITY
   - Encryption at rest (encrypt WAL and snapshot files)
   - Key expiration (don't keep sensitive data forever)
   - Audit logging for security-sensitive operations
```

**Q4: What would you change for 10x/100x scale?**
```
10x (10M keys, 100K ops/sec):
  - Switch to io_uring for network I/O (2-3x throughput)
  - Use jemalloc (LD_PRELOAD, zero code changes)
  - Batch WAL writes (group commit, fsync every N ms instead of per-write)
  - Connection pooling on client side

100x (1B keys, 10M ops/sec):
  - Shard across machines (consistent hashing)
  - In-memory only mode for hot data (WAL for durability, not performance)
  - DPDK/kernel bypass for networking
  - Custom allocator with memory-mapped files
  - Async replication with conflict resolution (CRDTs for counters/sets)
```

---

# 🎯 INTERVIEWER GUIDE — Set 13 KVStore Project Evaluation

---

## What This Project Demonstrates (Competency Map)

| Competency | How KVStore Shows It | Score Weight |
|-----------|---------------------|--------------|
| **Modern C++20** | Concepts, coroutines, expected, span | 15% |
| **Concurrency** | Sharded locks, atomic operations, thread pool | 20% |
| **Systems Design** | WAL, snapshots, TTL, protocol design | 25% |
| **Networking** | Async TCP, protocol parsing, graceful shutdown | 15% |
| **Testing** | Unit + integration tests, edge cases | 10% |
| **Build/Tooling** | CMake, vcpkg, sanitizers | 5% |
| **Production Readiness** | Error handling, logging, benchmarks | 10% |

## Discussion Questions Interviewers Will Ask

| Question | What They're Assessing |
|----------|------------------------|
| "Why sharded map instead of a single lock?" | Concurrency design understanding |
| "What's the failure mode if power is lost mid-WAL write?" | Crash recovery thinking |
| "How would you add replication?" | Distributed systems knowledge |
| "What's the bottleneck at 1M ops/sec?" | Performance analysis skills |
| "How would you test for race conditions?" | Testing methodology (TSan) |
| "Why text protocol instead of binary?" | Protocol design trade-offs |

## How This Maps to iCluster Experience

| KVStore Component | iCluster Equivalent | Interview Connection |
|-------------------|--------------------|-----------------------|
| Write-Ahead Log | QAUDJRN journal capture | "Both use append-only logs for crash recovery" |
| Sharded Map | HAWORKER parallelism | "Both partition work to reduce contention" |
| TTL Manager | Journal receiver cleanup | "Both manage lifecycle of time-bounded data" |
| Snapshot | Staging Store SAVF | "Both provide point-in-time persistent state" |
| TCP Protocol | DMKAPI communication | "Both define message framing with checksums" |
| Graceful shutdown | DMENDGRP quiesce | "Both drain in-flight operations before stopping" |

---

# 🎯 INTERVIEWER GUIDE — Set 13 KVStore Project Evaluation

---

## What This Project Demonstrates (Competency Map)

| Competency | How KVStore Shows It | Score Weight |
|-----------|---------------------|---------------|
| **Modern C++20** | Concepts, coroutines, expected, span | 15% |
| **Concurrency** | Sharded locks, atomic operations, thread pool | 20% |
| **Systems Design** | WAL, snapshots, TTL, protocol design | 25% |
| **Networking** | Async TCP, protocol parsing, graceful shutdown | 15% |
| **Testing** | Unit + integration tests, edge cases | 10% |
| **Build/Tooling** | CMake, vcpkg, sanitizers | 5% |
| **Production Readiness** | Error handling, logging, benchmarks | 10% |

## How Interviewers Evaluate Take-Home Projects

```
TIER 1 (Reject): Code compiles but doesn't work. No tests. No error handling.
TIER 2 (Weak): Works for happy path. Basic tests. Some error handling.
TIER 3 (Pass): Robust implementation. Good tests. Handles edge cases.
TIER 4 (Strong): Production-quality. Excellent tests. Performance-aware.
TIER 5 (Exceptional): All of Tier 4 + novel design decisions explained.
```

## Discussion Questions Interviewers Will Ask About This Project

| Question | What They're Assessing |
|----------|------------------------|
| "Why sharded map instead of a single lock?" | Concurrency design understanding |
| "What's the failure mode if power is lost mid-WAL write?" | Crash recovery thinking |
| "How would you add replication?" | Distributed systems knowledge |
| "What's the bottleneck at 1M ops/sec?" | Performance analysis skills |
| "How would you test for race conditions?" | Testing methodology (TSan, stress tests) |
| "Why text protocol instead of binary?" | Protocol design trade-offs |
| "How does TTL manager avoid scanning all keys?" | Algorithm design (lazy + active) |
| "What happens if snapshot takes 30 seconds?" | Consistency during long operations |

## How This Maps to iCluster Experience

| KVStore Component | iCluster Equivalent | Interview Connection |
|-------------------|--------------------|-----------------------|
| Write-Ahead Log | QAUDJRN journal capture | "Both use append-only logs for crash recovery" |
| Sharded Map | HAWORKER parallelism | "Both partition work to reduce contention" |
| TTL Manager | Journal receiver cleanup | "Both manage lifecycle of time-bounded data" |
| Snapshot | Staging Store SAVF | "Both provide point-in-time persistent state" |
| TCP Protocol | DMKAPI communication | "Both define message framing with checksums" |
| Graceful shutdown | DMENDGRP quiesce | "Both drain in-flight operations before stopping" |

## Key Design Decisions to Defend

```
Q: "Why 16 shards?"
A: "Empirically, core_count * 2 minimizes contention while avoiding
    excessive memory overhead. We can benchmark different counts."

Q: "Why not lock-free?"
A: "Per-shard mutex with shared_lock for reads gives us 95% of the
    throughput with 10% of the complexity. Lock-free is warranted only
    if profiling shows lock contention is the bottleneck."

Q: "Why WAL before in-memory update?"
A: "Crash consistency. If we crash after memory update but before WAL,
    we lose data. WAL-first guarantees durability at the cost of one
    fsync per write (or batched fsync for throughput)."

Q: "Why coroutines for the server?"
A: "One coroutine per connection gives us C10K scalability with
    sequential-looking code. No callback hell, no explicit state machines."
```

---
