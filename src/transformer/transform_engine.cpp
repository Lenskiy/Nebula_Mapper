#include "transformer/transform_engine.hpp"
#include <regex>
#include <sstream>
#include <iomanip>
#include <chrono>

namespace transformer {

TransformEngine::TransformEngine() {
    init_builtin_transforms();
}

void TransformEngine::init_builtin_transforms() {
    register_transform("time_format", time_transform);
    register_transform("price_normalize", price_transform);
    register_transform("string_normalize", string_transform);
    register_transform("array_join", array_join_transform);
    register_transform("to_boolean", boolean_transform);
}

void TransformEngine::register_transform(
    const std::string& name,
    TransformFunction transform) {
    transforms_[name] = std::move(transform);
}

Result<TransformValue> TransformEngine::apply_transform(
    const std::string& name,
    const TransformValue& value,
    const std::map<std::string, std::string>& params) {

    auto it = transforms_.find(name);
    if (it == transforms_.end()) {
        return TransformError{
            "Transform not found: " + name,
            std::nullopt,
            std::nullopt
        };
    }
    return it->second(value, params);
}

bool TransformEngine::has_transform(const std::string& name) const {
    return transforms_.find(name) != transforms_.end();
}

namespace detail {
    Result<std::string> format_time(const std::string& time_str, const std::string& format) {
        try {
            std::istringstream ss(time_str);
            std::tm tm = {};
            ss >> std::get_time(&tm, format.c_str());

            if (ss.fail()) {
                return TransformError{
                    "Failed to parse time string",
                    time_str,
                    std::nullopt
                };
            }

            std::ostringstream out;
            out << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
            return out.str();
        }
        catch (const std::exception& e) {
            return TransformError{
                "Error formatting time: " + std::string(e.what()),
                time_str,
                std::nullopt
            };
        }
    }

    Result<int64_t> parse_price(const std::string& price_str) {
        try {
            // Remove currency symbols and separators
            std::string clean_str;
            std::copy_if(price_str.begin(), price_str.end(),
                        std::back_inserter(clean_str),
                        [](char c) { return std::isdigit(c); });

            return std::stoll(clean_str);
        }
        catch (const std::exception& e) {
            return TransformError{
                "Error parsing price: " + std::string(e.what()),
                price_str,
                std::nullopt
            };
        }
    }

    Result<std::string> normalize_string(const std::string& input) {
        try {
            // Trim whitespace
            auto trimmed = trim(input);

            // Convert multiple spaces to single space
            std::regex spaces_regex("\\s+");
            std::string normalized = std::regex_replace(trimmed, spaces_regex, " ");

            return normalized;
        }
        catch (const std::exception& e) {
            return TransformError{
                "Error normalizing string: " + std::string(e.what()),
                input,
                std::nullopt
            };
        }
    }

    Result<bool> parse_boolean(const std::string& value) {
        std::string lower = value;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

        static const std::unordered_map<std::string, bool> BOOL_MAP = {
            {"true", true}, {"1", true}, {"yes", true},
            {"false", false}, {"0", false}, {"no", false}
        };

        auto it = BOOL_MAP.find(lower);
        if (it != BOOL_MAP.end()) {
            return it->second;
        }

        return TransformError{
            "Invalid boolean value",
            value,
            std::nullopt
        };
    }

    std::string trim(const std::string& str) {
        const auto first = str.find_first_not_of(" \t\n\r");
        if (first == std::string::npos) return "";
        const auto last = str.find_last_not_of(" \t\n\r");
        return str.substr(first, last - first + 1);
    }

    std::vector<std::string> split(const std::string& str, const std::string& delim) {
        std::vector<std::string> parts;
        size_t start = 0, end = 0;

        while ((end = str.find(delim, start)) != std::string::npos) {
            parts.push_back(trim(str.substr(start, end - start)));
            start = end + delim.length();
        }

        parts.push_back(trim(str.substr(start)));
        return parts;
    }

    std::string join(const std::vector<std::string>& parts, const std::string& delim) {
        if (parts.empty()) return "";

        std::ostringstream ss;
        auto it = parts.begin();
        ss << *it++;

        while (it != parts.end()) {
            ss << delim << *it++;
        }

        return ss.str();
    }

} // namespace detail

// Built-in transforms implementation
Result<TransformValue> TransformEngine::time_transform(
    const TransformValue& value,
    const std::map<std::string, std::string>& params) {

    auto format_it = params.find("format");
    if (format_it == params.end()) {
        return TransformError{"Missing required parameter: format"};
    }

    auto str_result = detail::convert_value<std::string>(value);
    if (std::holds_alternative<TransformError>(str_result)) {
        return std::get<TransformError>(str_result);
    }

    auto formatted = detail::format_time(std::get<std::string>(str_result), format_it->second);
    if (std::holds_alternative<TransformError>(formatted)) {
        return std::get<TransformError>(formatted);
    }

    TransformValue result;
    result.value = std::get<std::string>(formatted);
    result.source_type = "STRING";
    result.target_type = "TIMESTAMP";
    return result;
}

Result<TransformValue> TransformEngine::price_transform(
    const TransformValue& value,
    const std::map<std::string, std::string>&) {

    auto str_result = detail::convert_value<std::string>(value);
    if (std::holds_alternative<TransformError>(str_result)) {
        return std::get<TransformError>(str_result);
    }

    auto price = detail::parse_price(std::get<std::string>(str_result));
    if (std::holds_alternative<TransformError>(price)) {
        return std::get<TransformError>(price);
    }

    TransformValue result;
    result.value = std::get<int64_t>(price);
    result.source_type = "STRING";
    result.target_type = "INT64";
    return result;
}

Result<TransformValue> TransformEngine::string_transform(
    const TransformValue& value,
    const std::map<std::string, std::string>&) {

    auto str_result = detail::convert_value<std::string>(value);
    if (std::holds_alternative<TransformError>(str_result)) {
        return std::get<TransformError>(str_result);
    }

    auto normalized = detail::normalize_string(std::get<std::string>(str_result));
    if (std::holds_alternative<TransformError>(normalized)) {
        return std::get<TransformError>(normalized);
    }

    TransformValue result;
    result.value = std::get<std::string>(normalized);
    result.source_type = "STRING";
    result.target_type = "STRING";
    return result;
}

Result<TransformValue> TransformEngine::array_join_transform(
    const TransformValue& value,
    const std::map<std::string, std::string>& params) {

    std::string delimiter = params.find("delimiter") != params.end() ?
                           params.at("delimiter") : ",";

    auto str_result = detail::convert_value<std::string>(value);
    if (std::holds_alternative<TransformError>(str_result)) {
        return std::get<TransformError>(str_result);
    }

    auto parts = detail::split(std::get<std::string>(str_result), delimiter);
    auto joined = detail::join(parts, delimiter);

    TransformValue result;
    result.value = joined;
    result.source_type = "STRING";
    result.target_type = "STRING";
    return result;
}

Result<TransformValue> TransformEngine::boolean_transform(
    const TransformValue& value,
    const std::map<std::string, std::string>&) {

    auto str_result = detail::convert_value<std::string>(value);
    if (std::holds_alternative<TransformError>(str_result)) {
        return std::get<TransformError>(str_result);
    }

    auto bool_val = detail::parse_boolean(std::get<std::string>(str_result));
    if (std::holds_alternative<TransformError>(bool_val)) {
        return std::get<TransformError>(bool_val);
    }

    TransformValue result;
    result.value = std::get<bool>(bool_val);
    result.source_type = "STRING";
    result.target_type = "BOOL";
    return result;
}

} // namespace transformer