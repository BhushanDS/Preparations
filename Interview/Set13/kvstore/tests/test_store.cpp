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
    
    // Reopen store - should recover from WAL
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
