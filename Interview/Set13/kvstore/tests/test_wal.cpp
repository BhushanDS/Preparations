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
