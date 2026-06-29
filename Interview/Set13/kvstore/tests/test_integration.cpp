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
