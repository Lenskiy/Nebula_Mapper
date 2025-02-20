#ifndef NEBULA_MAPPER_TRANSFORM_ENGINE_HPP
#define NEBULA_MAPPER_TRANSFORM_ENGINE_HPP

#include "parser/mapping_parser.hpp"
#include <string>
#include <variant>
#include <optional>
#include <functional>
#include <map>
#include <unordered_map>

namespace transformer {

// Value that can be transformed
struct TransformValue {
    std::variant<std::string, int64_t, double, bool> value;
    std::string source_type;  // Original JSON type
    std::string target_type;  // Target Nebula type
};

// Transform error type
struct TransformError {
    std::string message;
    std::optional<std::string> context;
    std::optional<std::string> source_value;

    TransformError(const std::string& msg,
                  const std::optional<std::string>& ctx = std::nullopt,
                  const std::optional<std::string>& src = std::nullopt)
        : message(msg), context(ctx), source_value(src) {}
};

// Result type for transform operations
template<typename T>
using Result = std::variant<T, TransformError>;

// Transform function type
using TransformFunction = std::function<Result<TransformValue>(
    const TransformValue&,
    const std::map<std::string, std::string>&  // Parameters
)>;

namespace detail {
    // Type conversion utilities
    template<typename T>
    Result<T> convert_value(const TransformValue& value);

    // String manipulation utilities
    std::string trim(const std::string& str);
    std::vector<std::string> split(const std::string& str,
                                  const std::string& delim);
    std::string join(const std::vector<std::string>& parts,
                    const std::string& delim);

    // Value parsing and formatting
    Result<std::string> format_time(const std::string& time_str,
                                  const std::string& format);
    Result<int64_t> parse_price(const std::string& price_str);
    Result<std::string> normalize_string(const std::string& input);
    Result<bool> parse_boolean(const std::string& value);
}

class TransformEngine {
public:
    // Get singleton instance
    static TransformEngine& instance() {
        static TransformEngine engine;
        return engine;
    }

    // Register a new transform
    void register_transform(const std::string& name, TransformFunction transform);

    // Apply a transformation
    Result<TransformValue> apply_transform(
        const std::string& name,
        const TransformValue& value,
        const std::map<std::string, std::string>& params = {});

    // Verify if a transform exists
    bool has_transform(const std::string& name) const;

    // Built-in transforms
    static Result<TransformValue> time_transform(
        const TransformValue& value,
        const std::map<std::string, std::string>& params);

    static Result<TransformValue> price_transform(
        const TransformValue& value,
        const std::map<std::string, std::string>& params);

    static Result<TransformValue> string_transform(
        const TransformValue& value,
        const std::map<std::string, std::string>& params);

    static Result<TransformValue> array_join_transform(
        const TransformValue& value,
        const std::map<std::string, std::string>& params);

    static Result<TransformValue> boolean_transform(
        const TransformValue& value,
        const std::map<std::string, std::string>& params);

private:
    // Private constructor for singleton pattern
    TransformEngine();

    // Map of registered transforms
    std::map<std::string, TransformFunction> transforms_;

    // Initialize built-in transforms
    void init_builtin_transforms();

    // Delete copy and move operations
    TransformEngine(const TransformEngine&) = delete;
    TransformEngine& operator=(const TransformEngine&) = delete;
    TransformEngine(TransformEngine&&) = delete;
    TransformEngine& operator=(TransformEngine&&) = delete;
};

} // namespace transformer

// Include template implementation
#include "transform_engine.inl"

#endif // NEBULA_MAPPER_TRANSFORM_ENGINE_HPP