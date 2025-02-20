#include "graph/schema_manager.hpp"
#include "parser/mapping_parser.hpp"
#include <sstream>
#include <algorithm>
#include <unordered_set>
#include <iostream>

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

            element.properties.push_back(schema_prop);
        }

        // Validate schema element
        auto validation = validate_schema_element(element);
        if (std::holds_alternative<SchemaError>(validation)) {
            return std::get<SchemaError>(validation);
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

        // Convert properties and check if they should be indexed
        for (const auto& prop : vertex.properties) {
            SchemaProperty schema_prop;
            schema_prop.name = prop.name;
            schema_prop.type = prop.nebula_type;

            // Only index certain types
            if (detail::is_numeric_type(prop.nebula_type) ||
                detail::is_string_type(prop.nebula_type)) {
                schema_prop.indexable = true;
            }

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

            if (detail::is_numeric_type(prop.nebula_type) ||
                detail::is_string_type(prop.nebula_type)) {
                schema_prop.indexable = true;
            }

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

    // Merge edge constraints if applicable
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


SchemaResult<std::vector<std::string>> SchemaManager::generate_property_indexes(
    const SchemaElement& element) {

    std::vector<std::string> statements;

    for (const auto& prop : element.properties) {
        if (!prop.indexable) continue;

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

SchemaResult<std::string> SchemaManager::convert_to_nebula_type(
    const std::string& type,
    size_t string_length) {

    // Debug output
    std::cerr << "Converting type: " << type << " with length: " << string_length << std::endl;

    // Create a copy of the type string in uppercase for case-insensitive comparison
    std::string upper_type = type;
    std::transform(upper_type.begin(), upper_type.end(), upper_type.begin(), ::toupper);

    std::cerr << "Converted to uppercase: " << upper_type << std::endl;

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
        std::string result = upper_type + "(" + std::to_string(length) + ")";
        std::cerr << "Converted string type to: " << result << std::endl;
        return result;
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
        std::cerr << "Found type mapping: " << upper_type << " -> " << it->second << std::endl;
        return it->second;
    }

    // If the type is already a valid Nebula type, return it as is
    if (VALID_TYPES.find(upper_type) != VALID_TYPES.end()) {
        std::cerr << "Found valid type: " << upper_type << std::endl;
        return upper_type;
    }

    // Debug output before error
    std::cerr << "Type not found in mappings or valid types. Available mappings:" << std::endl;
    for (const auto& mapping : TYPE_MAP) {
        std::cerr << mapping.first << " -> " << mapping.second << std::endl;
    }
    std::cerr << "Valid types:" << std::endl;
    for (const auto& valid_type : VALID_TYPES) {
        std::cerr << valid_type << " ";
    }
    std::cerr << std::endl;

    return SchemaError{"Unsupported type: " + type};
}

bool SchemaManager::is_valid_identifier(const std::string& name) {
    if (name.empty() || name.length() > 128) {
        return false;
    }

    if (RESERVED_KEYWORDS.find(name) != RESERVED_KEYWORDS.end()) {
        return false;
    }

    if (!std::isalpha(name[0]) && name[0] != '_') {
        return false;
    }

    return std::all_of(name.begin() + 1, name.end(),
        [](char c) { return std::isalnum(c) || c == '_'; });
}

bool SchemaManager::is_valid_property_name(const std::string& name) {
    return is_valid_identifier(name);
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

        // Add debug output
        std::cerr << "Validating property: " << prop.name << " with type: " << prop.type << std::endl;

        // Extract base type from type specification
        std::string base_type = prop.type;
        size_t paren_pos = prop.type.find('(');
        if (paren_pos != std::string::npos) {
            base_type = prop.type.substr(0, paren_pos);
        }

        // Check if the base type is valid
        if (VALID_TYPES.find(base_type) == VALID_TYPES.end()) {
            // Add debug output
            std::cerr << "Available valid types: ";
            for (const auto& type : VALID_TYPES) {
                std::cerr << type << " ";
            }
            std::cerr << std::endl;

            return SchemaError{
                "Invalid property type: " + prop.type,
                prop.name
            };
        }

        // If it's a string type with length specification, validate the length
        if ((base_type == "STRING" || base_type == "FIXED_STRING") && paren_pos != std::string::npos) {
            try {
                size_t len_end = prop.type.find(')', paren_pos);
                if (len_end == std::string::npos) {
                    return SchemaError{
                        "Invalid string length specification: " + prop.type,
                        prop.name
                    };
                }

                std::string len_str = prop.type.substr(paren_pos + 1, len_end - paren_pos - 1);
                size_t length = std::stoull(len_str);

                if (length > 65535) {  // Nebula's maximum string length
                    return SchemaError{
                        "String length exceeds maximum allowed (65535): " + std::to_string(length),
                        prop.name
                    };
                }
            } catch (const std::exception& e) {
                return SchemaError{
                    "Invalid string length specification: " + prop.type,
                    prop.name
                };
            }
        }
    }

    return Success{};
}

namespace detail {

std::string escape_identifier(const std::string& name) {
    // Check if identifier needs escaping
    bool needs_escape = !std::isalpha(name[0]) && name[0] != '_';
    if (!needs_escape) {
        needs_escape = !std::all_of(name.begin() + 1, name.end(),
            [](char c) { return std::isalnum(c) || c == '_'; });
    }

    if (needs_escape) {
        return "`" + name + "`";
    }
    return name;
}

std::string get_index_name(const std::string& element_name,
                         const std::string& property_name) {
    return element_name + "_" + property_name + "_idx";
}

bool is_numeric_type(const std::string& type) {
    static const std::unordered_set<std::string> NUMERIC_TYPES = {
        "INT", "INT8", "INT16", "INT32", "INT64",
        "FLOAT", "DOUBLE"
    };

    // Extract base type without length specification
    std::string base_type = type;
    auto paren_pos = type.find('(');
    if (paren_pos != std::string::npos) {
        base_type = type.substr(0, paren_pos);
    }

    return NUMERIC_TYPES.find(base_type) != NUMERIC_TYPES.end();
}

bool is_string_type(const std::string& type) {
    static const std::unordered_set<std::string> STRING_TYPES = {
        "STRING", "FIXED_STRING", "VARCHAR"
    };

    // Extract base type without length specification
    std::string base_type = type;
    auto paren_pos = type.find('(');
    if (paren_pos != std::string::npos) {
        base_type = type.substr(0, paren_pos);
    }

    return STRING_TYPES.find(base_type) != STRING_TYPES.end();
}

} // namespace detail
} // namespace graph



#include "parser/mapping_parser.hpp"

namespace parser::mapping {

Result<GraphMapping> create_mapping(const parser::yaml::Result<YAML::Node>& config) {
    if (std::holds_alternative<parser::yaml::Error>(config)) {
        return Error{
            "Failed to parse YAML config: " +
            std::get<parser::yaml::Error>(config).message
        };
    }

    const auto& yaml_config = std::get<YAML::Node>(config);
    GraphMapping mapping;

    // Parse settings if present
    if (yaml_config["settings"]) {
        const auto& settings = yaml_config["settings"];
        if (settings["string_length"]) {
            mapping.settings.string_length = settings["string_length"].as<size_t>();
        }
        if (settings["array_delimiter"]) {
            mapping.settings.array_delimiter = settings["array_delimiter"].as<std::string>();
        }
        if (settings["dynamic_tags"]) {
            mapping.settings.allow_dynamic_tags = settings["dynamic_tags"].as<bool>();
        }
    }

    // Parse tags
    if (yaml_config["tags"]) {
        for (const auto& tag : yaml_config["tags"]) {
            auto tag_name = tag.first.as<std::string>();
            auto vertex_result = detail::create_vertex_mapping(
                tag.second.as<parser::yaml::TagMapping>(),
                tag_name
            );

            if (std::holds_alternative<Error>(vertex_result)) {
                return std::get<Error>(vertex_result);
            }

            mapping.vertices.push_back(std::get<VertexMapping>(vertex_result));
        }
    }

    // Parse edges
    if (yaml_config["edges"]) {
        for (const auto& edge : yaml_config["edges"]) {
            auto edge_name = edge.first.as<std::string>();
            auto edge_result = detail::create_edge_mapping(
                edge.second.as<parser::yaml::EdgeMapping>(),
                edge_name
            );

            if (std::holds_alternative<Error>(edge_result)) {
                return std::get<Error>(edge_result);
            }

            mapping.edges.push_back(std::get<EdgeMapping>(edge_result));
        }
    }

    return mapping;
}

namespace detail {

    Result<VertexMapping> create_vertex_mapping(
        const parser::yaml::TagMapping& tag_def,
        const std::string& tag_name) {

        VertexMapping vertex;
        vertex.tag_name = tag_name;
        vertex.source_path = tag_def.json_path;
        vertex.key_path = tag_def.key_field;
        vertex.dynamic_fields = tag_def.dynamic_fields.enabled;  // Access enabled flag

        // Convert properties
        for (const auto& [prop_name, prop_def] : tag_def.properties) {
            auto prop_result = create_property_mapping(prop_def, prop_name);
            if (std::holds_alternative<Error>(prop_result)) {
                return std::get<Error>(prop_result);
            }
            vertex.properties.push_back(std::get<Property>(prop_result));
        }

        return vertex;
    }

Result<EdgeMapping> create_edge_mapping(
    const parser::yaml::EdgeMapping& edge_def,
    const std::string& edge_name) {

    EdgeMapping edge;
    edge.edge_name = edge_name;
    edge.source_path = edge_def.json_path;
    edge.from.tag = edge_def.from.tag;
    edge.from.key_path = edge_def.from.key_field;
    edge.to.tag = edge_def.to.tag;
    edge.to.key_path = edge_def.to.key_field;

    // Convert properties
    for (const auto& [prop_name, prop_def] : edge_def.properties) {
        auto prop_result = create_property_mapping(prop_def, prop_name);
        if (std::holds_alternative<Error>(prop_result)) {
            return std::get<Error>(prop_result);
        }
        edge.properties.push_back(std::get<Property>(prop_result));
    }

    return edge;
}

Result<Property> create_property_mapping(
    const parser::yaml::PropertyMapping& prop_def,
    const std::string& prop_name) {

    Property prop;
    prop.name = prop_name;
    prop.json_path = prop_def.json_path;
    prop.nebula_type = prop_def.nebula_type;
    prop.optional = prop_def.optional;
    prop.default_value = prop_def.default_value;

    return prop;
}

} // namespace detail
} // namespace parser::mapping