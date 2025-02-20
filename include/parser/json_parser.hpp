#ifndef NEBULA_MAPPER_JSON_PARSER_HPP
#define NEBULA_MAPPER_JSON_PARSER_HPP

#include "common/result.hpp"
#include "common/string_utils.hpp"
#include <nlohmann/json.hpp>
#include <unordered_map>
#include <shared_mutex>

namespace parser::json {

using JsonDocument = nlohmann::json;

// JSON-specific error type
struct Error : common::Error {
    std::optional<size_t> line_number{std::nullopt};
    std::optional<size_t> column{std::nullopt};

    Error(const std::string& msg,
          std::optional<size_t> line = std::nullopt,
          std::optional<size_t> col = std::nullopt)
        : common::Error(msg), line_number(line), column(col) {}
};

template<typename T>
using Result = common::Result<T, Error>;

class JsonParser {
public:
    static JsonParser& instance() {
        static JsonParser parser;
        return parser;
    }

    // Parser functions
    Result<JsonDocument> parse(const std::string& input);
    Result<JsonDocument> parse_file(const std::string& file_path);

    // Value extraction with caching
    template<typename T>
    Result<T> get_value(const JsonDocument& j, const std::string& path);

    // Helper functions
    bool has_path(const JsonDocument& j, const std::string& path);
    Result<std::string> to_string(const JsonDocument& j);

    // Cache management
    void clear_cache();
    size_t cache_size() const;

private:
    JsonParser() = default;
    JsonParser(const JsonParser&) = delete;
    JsonParser& operator=(const JsonParser&) = delete;

    // Cache for parsed paths
    mutable std::shared_mutex cache_mutex_;
    std::unordered_map<std::string, std::vector<std::string>> path_cache_;

    // Helper functions
    Result<JsonDocument> navigate_path(
        const JsonDocument& j,
        const std::vector<std::string>& segments);

    // Path parsing with caching
    const std::vector<std::string>& get_parsed_path(const std::string& path);
};

// Template implementations
template<typename T>
Result<T> JsonParser::get_value(const JsonDocument& j, const std::string& path) {
    const auto& segments = get_parsed_path(path);
    auto result = navigate_path(j, segments);

    if (std::holds_alternative<Error>(result)) {
        return std::get<Error>(result);
    }

    const auto& value = std::get<JsonDocument>(result);
    try {
        return value.get<T>();
    } catch (const JsonDocument::exception& e) {
        return Error{"Type conversion failed: " + std::string(e.what())};
    }
}

// Convenience free functions
inline Result<JsonDocument> parse(const std::string& input) {
    return JsonParser::instance().parse(input);
}

inline Result<JsonDocument> parse_file(const std::string& file_path) {
    return JsonParser::instance().parse_file(file_path);
}

template<typename T>
inline Result<T> get_value(const JsonDocument& j, const std::string& path) {
    return JsonParser::instance().get_value<T>(j, path);
}

inline bool has_path(const JsonDocument& j, const std::string& path) {
    return JsonParser::instance().has_path(j, path);
}

inline Result<std::string> to_string(const JsonDocument& j) {
    return JsonParser::instance().to_string(j);
}

} // namespace parser::json

#endif // NEBULA_MAPPER_JSON_PARSER_HPP