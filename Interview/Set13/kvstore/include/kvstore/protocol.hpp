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
