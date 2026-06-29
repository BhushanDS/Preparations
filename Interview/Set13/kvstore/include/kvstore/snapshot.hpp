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
