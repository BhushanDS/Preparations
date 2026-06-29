#pragma once

#include "kvstore/common.hpp"
#include <filesystem>
#include <fstream>
#include <mutex>
#include <vector>
#include <cstring>

namespace kvstore {

// WAL File Format:
// Each entry: [4-byte length][WALOp][key_len][key][val_len][value][8-byte ttl][8-byte seq]
// Append-only - never modified, only appended or truncated after snapshot

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
