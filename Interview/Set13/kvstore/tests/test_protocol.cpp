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
