#include "graph/schema_manager.hpp"
#include <sstream>
#include <algorithm>
#include <unordered_set>

namespace graph {

    namespace {
        // Valid Nebula Graph types
        const std::unordered_set<std::string> VALID_TYPES = {
            "BOOL", "INT", "INT8", "INT16", "INT32", "INT64",
            "FLOAT", "DOUBLE", "STRING", "FIXED_STRING",
            "TIMESTAMP", "DATE", "TIME", "DATETIME"
        };

        // Default lengths for string types
        const std::unordered_map<std::string, size_t> DEFAULT_LENGTHS = {
            {"STRING", 256},
            {"FIXED_STRING", 32},
            {"VARCHAR", 256}
        };

        // Reserved keywords in Nebula Graph
        const std::unordered_set<std::string> RESERVED_KEYWORDS = {
            "SPACE", "TAG", "EDGE", "VERTEX", "INDEX",
            "INSERT", "UPDATE", "DELETE", "WHERE", "YIELD"
        };
    }


    SchemaResult<Success> SchemaManager::validate_schema_element(
        const SchemaElement& element) {
    if (!is_valid_identifier(element.name)) {
        return SchemaError{
            "Invalid schema element name: " + element.name
        };
    }

    for (const auto& prop : element.properties) {
        if (!is_valid_property_name(prop.name)) {
            return SchemaError{
                "Invalid property name: " + prop.name,
                element.name
            };
        }
    }

    return Success{};
}

SchemaResult<std::vector<std::string>> SchemaManager::generate_schema_statements(
    const parser::mapping::GraphMapping& mapping) {

    std::vector<std::string> statements;

    // Generate tag statements first
    for (const auto& vertex : mapping.vertices) {
        SchemaElement element;
        element.name = vertex.tag_name;
        element.is_edge = false;

        // Convert properties
        for (const auto& prop : vertex.properties) {
            SchemaProperty schema_prop;
            schema_prop.name = prop.name;

            auto type_result = convert_to_nebula_type(prop.nebula_type);
            if (std::holds_alternative<SchemaError>(type_result)) {
                return std::get<SchemaError>(type_result);
            }
            schema_prop.type = std::get<std::string>(type_result);

            schema_prop.nullable = prop.optional;
            schema_prop.default_value = prop.default_value;
            schema_prop.indexable = prop.indexable;  // Add indexable property

            element.properties.push_back(schema_prop);
        }

        // Validate schema element
        auto validation = validate_schema_element(element);
        if (std::holds_alternative<SchemaError>(validation)) {
            return std::get<SchemaError>(validation);
        }

        // Generate CREATE TAG statement
        std::stringstream ss;
        ss << "CREATE TAG IF NOT EXISTS " << detail::escape_identifier(vertex.tag_name) << " (\n";

        bool first = true;
        for (const auto& prop : element.properties) {
            if (!first) ss << ",\n";
            ss << "    " << detail::escape_identifier(prop.name)
               << " " << prop.type;

            if (!prop.nullable) {
                ss << " NOT NULL";
            }

            if (prop.default_value) {
                ss << " DEFAULT " << *prop.default_value;
            }
            first = false;
        }

        ss << "\n) ttl_duration = 0, ttl_col = \"\";";
        statements.push_back(ss.str());

        // Generate index statements for indexable properties
        for (const auto& prop : element.properties) {
            if (prop.indexable) {
                std::stringstream idx_ss;
                idx_ss << "CREATE TAG INDEX IF NOT EXISTS "
                      << detail::escape_identifier(vertex.tag_name + "_" + prop.name + "_idx")
                      << " ON " << detail::escape_identifier(vertex.tag_name)
                      << "(" << detail::escape_identifier(prop.name) << ");";
                statements.push_back(idx_ss.str());
            }
        }
    }

    // Generate edge statements
    for (const auto& edge : mapping.edges) {
        SchemaElement element;
        element.name = edge.edge_name;
        element.is_edge = true;

        // Store edge constraints
        element.edge_constraints.from_types.insert(edge.from.tag);
        element.edge_constraints.to_types.insert(edge.to.tag);

        // Convert properties
        for (const auto& prop : edge.properties) {
            SchemaProperty schema_prop;
            schema_prop.name = prop.name;

            auto type_result = convert_to_nebula_type(prop.nebula_type);
            if (std::holds_alternative<SchemaError>(type_result)) {
                return std::get<SchemaError>(type_result);
            }
            schema_prop.type = std::get<std::string>(type_result);

            schema_prop.nullable = prop.optional;
            schema_prop.default_value = prop.default_value;
            schema_prop.indexable = prop.indexable;  // Add indexable property

            element.properties.push_back(schema_prop);
        }

        // Generate CREATE EDGE statement
        std::stringstream ss;
        ss << "CREATE EDGE IF NOT EXISTS " << detail::escape_identifier(edge.edge_name) << " (\n";

        bool first = true;
        for (const auto& prop : element.properties) {
            if (!first) ss << ",\n";
            ss << "    " << detail::escape_identifier(prop.name)
               << " " << prop.type;

            if (!prop.nullable) {
                ss << " NOT NULL";
            }

            if (prop.default_value) {
                ss << " DEFAULT " << *prop.default_value;
            }
            first = false;
        }

        ss << "\n) ttl_duration = 0, ttl_col = \"\";";
        statements.push_back(ss.str());

        // Generate index statements for indexable edge properties
        for (const auto& prop : element.properties) {
            if (prop.indexable) {
                std::stringstream idx_ss;
                idx_ss << "CREATE EDGE INDEX IF NOT EXISTS "
                      << detail::escape_identifier(edge.edge_name + "_" + prop.name + "_idx")
                      << " ON " << detail::escape_identifier(edge.edge_name)
                      << "(" << detail::escape_identifier(prop.name) << ");";
                statements.push_back(idx_ss.str());
            }
        }
    }

    return statements;
}


    SchemaResult<std::vector<std::string>> SchemaManager::generate_property_indexes(
        const SchemaElement& element) {

        std::vector<std::string> statements;

        for (const auto& prop : element.properties) {
            if (!prop.indexable) continue;  // Changed from index to indexable

            // Skip non-indexable types
            if (!detail::is_numeric_type(prop.type) &&
                !detail::is_string_type(prop.type)) {
                continue;
                }

            std::stringstream ss;
            ss << "CREATE " << (element.is_edge ? "EDGE" : "TAG")
               << " INDEX IF NOT EXISTS "
               << detail::get_index_name(element.name, prop.name)
               << " ON " << detail::escape_identifier(element.name)
               << "(" << detail::escape_identifier(prop.name);

            // Add length for string indexes if specified
            if (detail::is_string_type(prop.type) && prop.fixed_length) {
                ss << "(" << *prop.fixed_length << ")";
            }

            ss << ");";
            statements.push_back(ss.str());
        }

        return statements;
    }


    SchemaResult<std::vector<std::string>> SchemaManager::generate_index_statements(
        const parser::mapping::GraphMapping& mapping) {
        std::vector<std::string> statements;

        // Generate indexes for tags
        for (const auto& vertex : mapping.vertices) {
            SchemaElement element;
            element.name = vertex.tag_name;
            element.is_edge = false;

            for (const auto& prop : vertex.properties) {
                SchemaProperty schema_prop;
                schema_prop.name = prop.name;
                schema_prop.type = prop.nebula_type;
                schema_prop.indexable = prop.indexable;  // Changed from index to indexable

                element.properties.push_back(schema_prop);
            }

            auto index_stmts = generate_property_indexes(element);
            if (std::holds_alternative<SchemaError>(index_stmts)) {
                return std::get<SchemaError>(index_stmts);
            }

            auto& new_stmts = std::get<std::vector<std::string>>(index_stmts);
            statements.insert(statements.end(), new_stmts.begin(), new_stmts.end());
        }

        // Generate indexes for edges
        for (const auto& edge : mapping.edges) {
            SchemaElement element;
            element.name = edge.edge_name;
            element.is_edge = true;

            for (const auto& prop : edge.properties) {
                SchemaProperty schema_prop;
                schema_prop.name = prop.name;
                schema_prop.type = prop.nebula_type;
                schema_prop.indexable = prop.indexable;  // Changed from index to indexable

                element.properties.push_back(schema_prop);
            }

            auto index_stmts = generate_property_indexes(element);
            if (std::holds_alternative<SchemaError>(index_stmts)) {
                return std::get<SchemaError>(index_stmts);
            }

            auto& new_stmts = std::get<std::vector<std::string>>(index_stmts);
            statements.insert(statements.end(), new_stmts.begin(), new_stmts.end());
        }

        return statements;
    }

    SchemaResult<std::vector<std::string>> SchemaManager::generate_cleanup_statements(
        const parser::mapping::GraphMapping& mapping) {

        std::vector<std::string> statements;

        // Drop indexes first
        for (const auto& vertex : mapping.vertices) {
            for (const auto& prop : vertex.properties) {
                statements.push_back(
                    "DROP TAG INDEX IF EXISTS " +
                    detail::get_index_name(vertex.tag_name, prop.name) + ";"
                );
            }
        }

        for (const auto& edge : mapping.edges) {
            for (const auto& prop : edge.properties) {
                statements.push_back(
                    "DROP EDGE INDEX IF EXISTS " +
                    detail::get_index_name(edge.edge_name, prop.name) + ";"
                );
            }
        }

        // Then drop tags and edges
        for (const auto& vertex : mapping.vertices) {
            statements.push_back(
                "DROP TAG IF EXISTS " + detail::escape_identifier(vertex.tag_name) + ";"
            );
        }

        for (const auto& edge : mapping.edges) {
            statements.push_back(
                "DROP EDGE IF EXISTS " + detail::escape_identifier(edge.edge_name) + ";"
            );
        }

        return statements;
    }

    SchemaResult<SchemaElement> SchemaManager::merge_schema_properties(
     const SchemaElement& existing,
     const SchemaElement& new_schema) {

        if (existing.name != new_schema.name || existing.is_edge != new_schema.is_edge) {
            return SchemaError{
                "Schema elements do not match",
                existing.name + " vs " + new_schema.name
            };
        }

        SchemaElement merged = existing;
        std::unordered_map<std::string, size_t> prop_map;

        // Index existing properties
        for (size_t i = 0; i < existing.properties.size(); ++i) {
            prop_map[existing.properties[i].name] = i;
        }

        // Merge new properties
        for (const auto& new_prop : new_schema.properties) {
            auto it = prop_map.find(new_prop.name);
            if (it == prop_map.end()) {
                // Add new property
                merged.properties.push_back(new_prop);
            } else {
                // Merge property attributes
                auto& existing_prop = merged.properties[it->second];
                existing_prop.nullable |= new_prop.nullable;
                if (new_prop.default_value) {
                    existing_prop.default_value = new_prop.default_value;
                }
                if (new_prop.fixed_length) {
                    existing_prop.fixed_length = std::max(
                        existing_prop.fixed_length.value_or(0),
                        new_prop.fixed_length.value()
                    );
                }
            }
        }

        // Merge edge constraints
        if (merged.is_edge) {
            merged.edge_constraints.from_types.insert(
                new_schema.edge_constraints.from_types.begin(),
                new_schema.edge_constraints.from_types.end()
            );
            merged.edge_constraints.to_types.insert(
                new_schema.edge_constraints.to_types.begin(),
                new_schema.edge_constraints.to_types.end()
            );
        }

        return merged;
    }




    SchemaResult<std::string> SchemaManager::convert_to_nebula_type(
        const std::string& type,
        size_t string_length) {

        // Create a copy of the type string in uppercase for case-insensitive comparison
        std::string upper_type = type;
        std::transform(upper_type.begin(), upper_type.end(), upper_type.begin(), ::toupper);

        // Handle fixed-length string types
        if (upper_type == "STRING" || upper_type == "FIXED_STRING" || upper_type == "VARCHAR") {
            auto it = DEFAULT_LENGTHS.find(upper_type);
            size_t length = string_length > 0 ? string_length :
                           (it != DEFAULT_LENGTHS.end() ? it->second : 256);

            if (length > 65535) {  // Nebula's maximum string length
                return SchemaError{
                    "String length exceeds maximum allowed: " +
                    std::to_string(length)
                };
            }
            return upper_type + "(" + std::to_string(length) + ")";
        }

        // Handle numeric and other types
        static const std::unordered_map<std::string, std::string> TYPE_MAP = {
            {"INT", "INT64"},
            {"INTEGER", "INT64"},
            {"FLOAT", "DOUBLE"},
            {"DOUBLE", "DOUBLE"},
            {"BOOL", "BOOL"},
            {"BOOLEAN", "BOOL"},
            {"TIMESTAMP", "TIMESTAMP"},
            {"DATE", "DATE"},
            {"TIME", "TIME"},
            {"DATETIME", "DATETIME"}
        };

        auto it = TYPE_MAP.find(upper_type);
        if (it != TYPE_MAP.end()) {
            return it->second;
        }

        // If the type is already a valid Nebula type, return it as is
        if (VALID_TYPES.find(upper_type) != VALID_TYPES.end()) {
            return upper_type;
        }

        return SchemaError{"Unsupported type: " + type};
    }


    bool SchemaManager::is_valid_identifier(const std::string& name) {
    if (name.empty() || name.length() > 128) {
        return false;
    }

    // Check if it's a reserved keyword
    if (RESERVED_KEYWORDS.find(name) != RESERVED_KEYWORDS.end()) {
        return false;
    }

    // First character must be letter or underscore
    if (!std::isalpha(name[0]) && name[0] != '_') {
        return false;
    }

    // Rest can be letters, numbers, or underscore
    return std::all_of(name.begin() + 1, name.end(),
        [](char c) { return std::isalnum(c) || c == '_'; });
}

bool SchemaManager::is_valid_property_name(const std::string& name) {
    return is_valid_identifier(name);
}


namespace detail {

std::string escape_identifier(const std::string& name) {
    return "`" + name + "`";
}

std::string get_index_name(
    const std::string& element_name,
    const std::string& property_name) {

    return element_name + "_" + property_name + "_idx";
}

bool is_numeric_type(const std::string& type) {
    static const std::unordered_set<std::string> NUMERIC_TYPES = {
        "INT", "INT8", "INT16", "INT32", "INT64",
        "FLOAT", "DOUBLE"
    };
    return NUMERIC_TYPES.find(type) != NUMERIC_TYPES.end();
}

bool is_string_type(const std::string& type) {
    return type.find("STRING") != std::string::npos ||
           type.find("VARCHAR") != std::string::npos;
}

} // namespace detail

} // namespace graph