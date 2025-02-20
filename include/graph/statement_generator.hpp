#ifndef NEBULA_MAPPER_STATEMENT_GENERATOR_HPP
#define NEBULA_MAPPER_STATEMENT_GENERATOR_HPP

#include "common/result.hpp"
#include "parser/mapping_parser.hpp"
#include "parser/json_parser.hpp"

namespace graph {

// Types of statements that can be generated
enum class StatementType {
    INSERT_VERTEX,
    INSERT_EDGE,
    UPDATE_VERTEX,
    UPDATE_EDGE,
    DELETE_VERTEX,
    DELETE_EDGE
};

// Represents a value to be inserted
struct Value {
    std::string nebula_type;
    std::variant<std::string, int64_t, double, bool> value;
    bool is_null{false};
};

// Error type for statement generation
struct StatementError : common::Error {
    std::optional<std::string> json_path{std::nullopt};

    StatementError(const std::string& msg,
                  const std::optional<std::string>& ctx = std::nullopt,
                  const std::optional<std::string>& path = std::nullopt)
        : common::Error(msg, ctx), json_path(path) {}
};

// Result type for operations that can fail
template<typename T>
using Result = common::Result<T, StatementError>;

class StatementGenerator {
public:
    // Generate statements from JSON data using mapping
    Result<std::vector<std::string>> generate_statements(
        const parser::mapping::GraphMapping& mapping,
        const parser::json::JsonDocument& data);

    // Generate batch insert statements
    Result<std::vector<std::string>> generate_batch_statements(
        const parser::mapping::GraphMapping& mapping,
        const parser::json::JsonDocument& data,
        size_t batch_size = 500);

private:
    // Fixed method declarations without class qualification
    std::string infer_type(const parser::json::JsonDocument& value);

    void process_dynamic_properties(
        const parser::json::JsonDocument& vertex,
        const parser::mapping::VertexMapping& vertex_mapping,
        std::vector<std::string>& prop_names,
        std::vector<std::string>& prop_values,
        const std::set<std::string>& defined_properties);

    // Helper methods for statement generation
    void generate_insert_vertex_statement(
        std::vector<std::string>& statements,
        const std::string& tag_name,
        const std::vector<std::string>& prop_names,
        const std::vector<std::string>& batch_values);

    void generate_insert_edge_statement(
        std::vector<std::string>& statements,
        const std::string& edge_name,
        const std::vector<std::string>& prop_names,
        const std::vector<std::string>& batch_values);

    Result<std::vector<parser::json::JsonDocument>> get_array_or_single(
        const parser::json::JsonDocument& data,
        const std::string& path);

    Result<Value> extract_value(
        const parser::json::JsonDocument& data,
        const std::string& json_path,
        const std::string& nebula_type,
        const std::optional<parser::mapping::Transform>& transform = std::nullopt);

    Result<std::string> format_value(const Value& value);

    Result<std::string> get_vertex_id(
        const parser::json::JsonDocument& data,
        const std::string& key_path);



    static std::string escape_string(const std::string& str);
    static std::string quote_identifier(const std::string& identifier);
};

namespace detail {
    // Helper functions
    std::string join_values(const std::vector<std::string>& values,
                          const std::string& delimiter = ", ");

    std::string build_property_list(
        const std::vector<std::pair<std::string, std::string>>& properties);

    Result<std::string> format_timestamp(const std::string& value);
    Result<std::string> format_date(const std::string& value);
    Result<std::string> format_datetime(const std::string& value);
}

} // namespace graph

#endif // NEBULA_MAPPER_STATEMENT_GENERATOR_HPP