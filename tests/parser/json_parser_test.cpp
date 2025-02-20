#include <gtest/gtest.h>
#include "parser/json_parser.hpp"

namespace {

class JsonParserTest : public ::testing::Test {
protected:
    void SetUp() override {
        const char* json_data = R"({
            "basicInfo": {
                "cid": 1081433159,
                "placenamefull": "요고 프로즌요거트 대흥역점",
                "wpointx": 487529,
                "wpointy": 1124303,
                "phonenum": "070-7655-4177",
                "mainphotourl": "http://t1.kakaocdn.net/mystore/713019FFD16345828078AB8939AFDD9A"
            },
            "comment": {
                "list": [
                    {
                        "date": "2024.09.25.",
                        "point": 5,
                        "likeCnt": 0,
                        "contents": "요아정보다 맛있음",
                        "commentid": "11081845",
                        "strengths": [
                            {
                                "id": 5,
                                "name": "맛"
                            },
                            {
                                "id": 1,
                                "name": "가성비"
                            }
                        ]
                    }
                ],
                "scorecnt": 17,
                "scoresum": 83
            }
        })";
        test_json = parser::json::parse(json_data);
        ASSERT_TRUE(std::holds_alternative<parser::json::JsonDocument>(test_json));
    }

    parser::json::Result<parser::json::JsonDocument> test_json;
};

// Test basic JSON parsing
TEST_F(JsonParserTest, ParsesValidJson) {
    const std::string input = R"({"test": "value"})";
    auto result = parser::json::parse(input);
    ASSERT_TRUE(std::holds_alternative<parser::json::JsonDocument>(result));
    const auto& json = std::get<parser::json::JsonDocument>(result);
    EXPECT_EQ(json["test"], "value");
}

// Test invalid JSON handling
TEST_F(JsonParserTest, HandlesInvalidJson) {
    const std::string input = R"({"invalid": "json")";
    auto result = parser::json::parse(input);
    ASSERT_TRUE(std::holds_alternative<parser::json::Error>(result));
}

// Test value extraction
TEST_F(JsonParserTest, ExtractsValues) {
    const auto& json = std::get<parser::json::JsonDocument>(test_json);
    auto cid = parser::json::get_value<int>(json, "/basicInfo/cid");
    ASSERT_TRUE(std::holds_alternative<int>(cid));
    EXPECT_EQ(std::get<int>(cid), 1081433159);
}

// Test path checking
TEST_F(JsonParserTest, ChecksPathExistence) {
    const auto& json = std::get<parser::json::JsonDocument>(test_json);
    EXPECT_TRUE(parser::json::has_path(json, "/basicInfo/cid"));
    EXPECT_FALSE(parser::json::has_path(json, "/nonexistent"));
}

// Test default value handling
TEST_F(JsonParserTest, HandlesDefaultValues) {
    const auto& json = std::get<parser::json::JsonDocument>(test_json);
    EXPECT_EQ(parser::json::get_value_or(json, "/basicInfo/cid", -1), 1081433159);
    EXPECT_EQ(parser::json::get_value_or(json, "/nonexistent", -1), -1);
}

} // namespace