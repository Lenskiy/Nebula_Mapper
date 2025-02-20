#ifndef NEBULA_MAPPER_SCHEMA_MANAGER_HPP
#define NEBULA_MAPPER_SCHEMA_MANAGER_HPP

#include "common/result.hpp"
#include "parser/mapping_parser.hpp"
#include <set>
#include <unordered_map>

namespace graph {

// Schema property definition
struct SchemaProperty {
    std::string name;
    std::string type;
    bool nullable{false};
    bool indexable{false};  // Add this line
    std::optional<std::string> default_value;
    std::optional<size_t> fixed_length;  // For FIXED_STRING
};

// Schema element (Tag or Edge) definition
struct SchemaElement {
    std::string name;
    std::vector<SchemaProperty> properties;
    bool is_edge{false};

    // For edges only
    struct {
        std::set<std::string> from_types;  // Valid source vertex types
        std::set<std::string> to_types;    // Valid target vertex types
    } edge_constraints;
};

// Error type for schema operations
struct SchemaError : public common::Error {
    SchemaError(const std::string& msg,
              const std::optional<std::string>& ctx = std::nullopt)
        : common::Error(msg, ctx) {}
};

// Schema-specific result type
template<typename T>
using SchemaResult = common::Result<T, SchemaError>;

// Empty type for void results
struct Success {};

class SchemaManager {
public:
    // Generate schema statements from mapping
    SchemaResult<std::vector<std::string>> generate_schema_statements(
        const parser::mapping::GraphMapping& mapping);

    // Generate index statements
    SchemaResult<std::vector<std::string>> generate_index_statements(
        const parser::mapping::GraphMapping& mapping);

    // Merge schema properties
    SchemaResult<SchemaElement> merge_schema_properties(
        const SchemaElement& existing,
        const SchemaElement& new_schema);

    // Generate cleanup statements
    SchemaResult<std::vector<std::string>> generate_cleanup_statements(
        const parser::mapping::GraphMapping& mapping);

private:
    // Helper functions
    SchemaResult<std::vector<std::string>> generate_property_indexes(
        const SchemaElement& element);

    SchemaResult<std::string> convert_to_nebula_type(
        const std::string& type,
        size_t string_length = 256);

    // Validation helpers
    static bool is_valid_identifier(const std::string& name);
    static bool is_valid_property_name(const std::string& name);
    SchemaResult<Success> validate_schema_element(const SchemaElement& element);
};

namespace detail {
    // Helper functions
    std::string escape_identifier(const std::string& name);
    std::string get_index_name(const std::string& element_name,
                             const std::string& property_name);
    bool is_numeric_type(const std::string& type);
    bool is_string_type(const std::string& type);
}

} // namespace graph

#endif // NEBULA_MAPPER_SCHEMA_MANAGER_HPP