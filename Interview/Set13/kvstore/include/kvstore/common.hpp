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
