#ifndef NEBULA_MAPPER_JSON_PARSER_HPP
#define NEBULA_MAPPER_JSON_PARSER_HPP

#include "common/result.hpp"
#include <nlohmann/json.hpp>

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

    // Use common Result with JSON Error
    template<typename T>
    using Result = common::Result<T, Error>;

    // Parser functions
    Result<JsonDocument> parse(const std::string& input);
    Result<JsonDocument> parse_file(const std::string& file_path);

    // Value extraction
    template<typename T>
    Result<T> get_value(const JsonDocument& j, const std::string& path);

    template<typename T>
    T get_value_or(const JsonDocument& j, const std::string& path, const T& default_value);

    bool has_path(const JsonDocument& j, const std::string& path);
    Result<std::string> to_string(const JsonDocument& j);

    namespace detail {
        std::vector<std::string> split_path(const std::string& path);
        Result<JsonDocument> navigate_path(const JsonDocument& j,
                                         const std::vector<std::string>& segments);
    }

} // namespace parser::json

// Template implementations
#include "json_parser.inl"

#endif