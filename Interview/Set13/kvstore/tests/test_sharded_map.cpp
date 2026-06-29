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
