#include "graph/statement_generator.hpp"
#include "transformer/transform_engine.hpp"
#include <unordered_set>
#include <sstream>
#include <regex>

namespace graph {

    void StatementGenerator::generate_insert_vertex_statement(
    std::vector<std::string>& statements,
    const std::string& tag_name,
    const std::vector<std::string>& prop_names,
    const std::vector<std::string>& batch_values) {

    std::stringstream ss;
    ss << "INSERT VERTEX " << quote_identifier(tag_name)
       << " (" << detail::join_values(prop_names) << ") "
       << "VALUES " << detail::join_values(batch_values) << ";";
    statements.push_back(ss.str());
}

void StatementGenerator::generate_insert_edge_statement(
    std::vector<std::string>& statements,
    const std::string& edge_name,
    const std::vector<std::string>& prop_names,
    const std::vector<std::string>& batch_values) {

    std::stringstream ss;
    ss << "INSERT EDGE " << quote_identifier(edge_name)
       << " (" << detail::join_values(prop_names) << ") "
       << "VALUES " << detail::join_values(batch_values) << ";";
    statements.push_back(ss.str());
}

Result<std::vector<parser::json::JsonDocument>> StatementGenerator::get_array_or_single(
    const parser::json::JsonDocument& data,
    const std::string& path) {

    try {
        auto result = parser::json::get_value<parser::json::JsonDocument>(data, path);
        if (std::holds_alternative<parser::json::Error>(result)) {
            return StatementError{
                "Failed to extract data: " +
                std::get<parser::json::Error>(result).message,
                path
            };
        }

        const auto& value = std::get<parser::json::JsonDocument>(result);
        std::vector<parser::json::JsonDocument> items;

        // Handle both array and single value cases
        if (value.is_array()) {
            items.insert(items.end(), value.begin(), value.end());
        } else {
            items.push_back(value);
        }

        return items;
    } catch (const std::exception& e) {
        return StatementError{
            "Error processing path: " + std::string(e.what()),
            path
        };
    }
}


Result<std::vector<std::string>> StatementGenerator::generate_batch_statements(
    const parser::mapping::GraphMapping& mapping,
    const parser::json::JsonDocument& data,
    size_t batch_size) {

    std::vector<std::string> statements;
    std::unordered_map<std::string, std::unordered_set<std::string>> processed_vertices;

    // Process vertices first
    for (const auto& vertex_mapping : mapping.vertices) {
        auto vertex_data = get_array_or_single(data, vertex_mapping.source_path);
        if (std::holds_alternative<StatementError>(vertex_data)) {
            return std::get<StatementError>(vertex_data);
        }

        const auto& vertices = std::get<std::vector<parser::json::JsonDocument>>(vertex_data);
        std::vector<std::string> batch_values;
        std::vector<std::string> prop_names;  // Moved inside the loop

        // Prepare property names once
        for (const auto& prop : vertex_mapping.properties) {
            prop_names.push_back(quote_identifier(prop.name));
        }

        // Process each vertex
        for (const auto& vertex : vertices) {
            auto vertex_id = get_vertex_id(vertex, vertex_mapping.key_path);
            if (std::holds_alternative<StatementError>(vertex_id)) {
                return std::get<StatementError>(vertex_id);
            }

            const std::string& id_str = std::get<std::string>(vertex_id);

            // Skip if we've already processed this vertex (for arrays)
            if (vertex_mapping.dynamic_fields.enabled) {  // Changed from allow_dynamic_fields
                auto& processed = processed_vertices[vertex_mapping.tag_name];
                if (processed.find(id_str) != processed.end()) {
                    continue;
                }
                processed.insert(id_str);
            }

            std::vector<std::string> prop_values;

            // Extract and format properties
            for (const auto& prop : vertex_mapping.properties) {
                auto value = extract_value(
                    vertex,
                    prop.json_path,
                    prop.nebula_type,
                    prop.transform
                );

                if (std::holds_alternative<StatementError>(value)) {
                    return std::get<StatementError>(value);
                }

                auto formatted = format_value(std::get<Value>(value));
                if (std::holds_alternative<StatementError>(formatted)) {
                    return std::get<StatementError>(formatted);
                }

                prop_values.push_back(std::get<std::string>(formatted));
            }

            // Generate UPSERT statement for vertices with dynamic fields
            if (vertex_mapping.dynamic_fields.enabled) {  // Changed from allow_dynamic_fields
                std::stringstream ss;
                ss << "UPSERT VERTEX " << quote_identifier(vertex_mapping.tag_name)
                   << " " << id_str << " ("
                   << detail::join_values(prop_names) << ") "
                   << "VALUES ("
                   << detail::join_values(prop_values) << ");";
                statements.push_back(ss.str());
            } else {
                batch_values.push_back(
                    id_str + ":(" +
                    detail::join_values(prop_values) + ")"
                );

                if (batch_values.size() >= batch_size) {
                    std::stringstream ss;
                    ss << "INSERT VERTEX " << quote_identifier(vertex_mapping.tag_name)
                       << " (" << detail::join_values(prop_names) << ") "
                       << "VALUES " << detail::join_values(batch_values) << ";";
                    statements.push_back(ss.str());
                    batch_values.clear();
                }
            }
        }

        // Handle remaining vertices
        if (!batch_values.empty()) {
            std::stringstream ss;
            ss << "INSERT VERTEX " << quote_identifier(vertex_mapping.tag_name)
               << " (" << detail::join_values(prop_names) << ") "
               << "VALUES " << detail::join_values(batch_values) << ";";
            statements.push_back(ss.str());
        }
    }

    // Process edges
    for (const auto& edge_mapping : mapping.edges) {
        auto edge_data = get_array_or_single(data, edge_mapping.source_path);
        if (std::holds_alternative<StatementError>(edge_data)) {
            return std::get<StatementError>(edge_data);
        }

        const auto& edges = std::get<std::vector<parser::json::JsonDocument>>(edge_data);
        std::vector<std::string> batch_values;
        std::vector<std::string> prop_names;

        // Prepare property names once
        for (const auto& prop : edge_mapping.properties) {
            prop_names.push_back(quote_identifier(prop.name));
        }

        // Process each edge
        for (const auto& edge : edges) {
            auto src_id = get_vertex_id(edge, edge_mapping.from.key_path);
            if (std::holds_alternative<StatementError>(src_id)) {
                return std::get<StatementError>(src_id);
            }

            auto dst_id = get_vertex_id(edge, edge_mapping.to.key_path);
            if (std::holds_alternative<StatementError>(dst_id)) {
                return std::get<StatementError>(dst_id);
            }

            std::vector<std::string> prop_values;
            for (const auto& prop : edge_mapping.properties) {
                auto value = extract_value(
                    edge,
                    prop.json_path,
                    prop.nebula_type,
                    prop.transform
                );

                if (std::holds_alternative<StatementError>(value)) {
                    return std::get<StatementError>(value);
                }

                auto formatted = format_value(std::get<Value>(value));
                if (std::holds_alternative<StatementError>(formatted)) {
                    return std::get<StatementError>(formatted);
                }

                prop_values.push_back(std::get<std::string>(formatted));
            }

            batch_values.push_back(
                std::get<std::string>(src_id) + " -> " +
                std::get<std::string>(dst_id) + ":(" +
                detail::join_values(prop_values) + ")"
            );

            if (batch_values.size() >= batch_size) {
                std::stringstream ss;
                ss << "INSERT EDGE " << quote_identifier(edge_mapping.edge_name)
                   << " (" << detail::join_values(prop_names) << ") "
                   << "VALUES " << detail::join_values(batch_values) << ";";
                statements.push_back(ss.str());
                batch_values.clear();
            }
        }

        // Handle remaining edges
        if (!batch_values.empty()) {
            std::stringstream ss;
            ss << "INSERT EDGE " << quote_identifier(edge_mapping.edge_name)
               << " (" << detail::join_values(prop_names) << ") "
               << "VALUES " << detail::join_values(batch_values) << ";";
            statements.push_back(ss.str());
        }
    }

    return statements;
}

std::string StatementGenerator::infer_type(const parser::json::JsonDocument& value) {
    if (value.is_boolean()) return "BOOL";
    if (value.is_number_integer()) return "INT64";
    if (value.is_number_float()) return "DOUBLE";
    if (value.is_string()) return "STRING";
    return "STRING";  // Default to STRING for unknown types
}


    void StatementGenerator::process_dynamic_properties(
        const parser::json::JsonDocument& vertex,
        const parser::mapping::VertexMapping& vertex_mapping,
        std::vector<std::string>& prop_names,
        std::vector<std::string>& prop_values,
        const std::set<std::string>& defined_properties) {

        if (!vertex_mapping.dynamic_fields.enabled) return;

        for (const auto& [key, value] : vertex.items()) {
            // Skip if property is already defined or excluded
            if (defined_properties.find(key) != defined_properties.end() ||
                vertex_mapping.dynamic_fields.excluded_properties.find(key) !=
                vertex_mapping.dynamic_fields.excluded_properties.end()) {
                continue;
                }

            std::string nebula_type = infer_type(value);

            // Skip if type is not allowed
            if (!vertex_mapping.dynamic_fields.allowed_types.empty() &&
                vertex_mapping.dynamic_fields.allowed_types.find(nebula_type) ==
                vertex_mapping.dynamic_fields.allowed_types.end()) {
                continue;
                }

            // Format value based on inferred type
            Value formatted_value;
            formatted_value.nebula_type = nebula_type;
            if (value.is_string()) {
                formatted_value.value = value.get<std::string>();
            } else if (value.is_number_integer()) {
                formatted_value.value = value.get<int64_t>();
            } else if (value.is_number_float()) {
                formatted_value.value = value.get<double>();
            } else if (value.is_boolean()) {
                formatted_value.value = value.get<bool>();
            } else {
                continue;  // Skip unsupported types
            }

            auto formatted = format_value(formatted_value);
            if (std::holds_alternative<StatementError>(formatted)) {
                continue;
            }

            prop_names.push_back(quote_identifier(key));
            prop_values.push_back(std::get<std::string>(formatted));
        }
    }

Result<Value> StatementGenerator::extract_value(
    const parser::json::JsonDocument& data,
    const std::string& json_path,
    const std::string& nebula_type,
    const std::optional<parser::mapping::Transform>& transform) {

    try {
        auto json_value = parser::json::get_value<parser::json::JsonDocument>(data, json_path);
        if (std::holds_alternative<parser::json::Error>(json_value)) {
            const auto& error = std::get<parser::json::Error>(json_value);
            return StatementError{
                "Failed to extract value: " + error.message,
                std::nullopt,
                json_path
            };
        }

        Value value;
        value.nebula_type = nebula_type;

        const auto& extracted = std::get<parser::json::JsonDocument>(json_value);
        if (extracted.is_null()) {
            value.is_null = true;
            return value;
        }

        // Apply transformation if specified
        if (transform) {
            transformer::TransformValue transform_input;

            // Convert the extracted value to TransformValue
            if (extracted.is_string()) {
                transform_input.value = extracted.get<std::string>();
                transform_input.source_type = "STRING";
            } else if (extracted.is_number_integer()) {
                transform_input.value = extracted.get<int64_t>();
                transform_input.source_type = "INT64";
            } else if (extracted.is_number_float()) {
                transform_input.value = extracted.get<double>();
                transform_input.source_type = "DOUBLE";
            } else if (extracted.is_boolean()) {
                transform_input.value = extracted.get<bool>();
                transform_input.source_type = "BOOL";
            } else {
                return StatementError{
                    "Unsupported value type for transformation",
                    json_path
                };
            }
            transform_input.target_type = nebula_type;

            // Apply transformation
            auto transform_result = transformer::TransformEngine::instance()
                .apply_transform(transform->type, transform_input, transform->params);

            if (std::holds_alternative<transformer::TransformError>(transform_result)) {
                const auto& error = std::get<transformer::TransformError>(transform_result);
                return StatementError{
                    "Transform error: " + error.message,
                    json_path
                };
            }

            const auto& transformed = std::get<transformer::TransformValue>(transform_result);
            value.value = transformed.value;
            return value;
        }

        // If no transformation, convert value based on Nebula type
        try {
            if (nebula_type == "INT" || nebula_type == "INT64") {
                value.value = extracted.get<int64_t>();
            }
            else if (nebula_type == "DOUBLE") {
                value.value = extracted.get<double>();
            }
            else if (nebula_type == "BOOL") {
                value.value = extracted.get<bool>();
            }
            else {
                value.value = extracted.get<std::string>();
            }
        }
        catch (const std::exception& e) {
            return StatementError{
                "Value conversion error: " + std::string(e.what()),
                json_path
            };
        }

        return value;
    }
    catch (const std::exception& e) {
        return StatementError{
            "Unexpected error: " + std::string(e.what()),
            json_path
        };
    }
}

Result<std::string> StatementGenerator::get_vertex_id(
    const parser::json::JsonDocument& data,
    const std::string& key_path) {

    auto id_value = parser::json::get_value<parser::json::JsonDocument>(data, key_path);
    if (std::holds_alternative<parser::json::Error>(id_value)) {
        const auto& error = std::get<parser::json::Error>(id_value);
        return StatementError{
            "Failed to extract vertex ID: " + error.message,
            std::nullopt,
            key_path
        };
    }

    const auto& extracted = std::get<parser::json::JsonDocument>(id_value);
    if (extracted.is_null()) {
        return StatementError{
            "Vertex ID cannot be null",
            key_path
        };
    }

    std::string id_str;
    try {
        if (extracted.is_string()) {
            id_str = extracted.get<std::string>();
        }
        else if (extracted.is_number()) {
            id_str = std::to_string(extracted.get<int64_t>());
        }
        else {
            return StatementError{
                "Invalid vertex ID type",
                key_path
            };
        }
    }
    catch (const std::exception& e) {
        return StatementError{
            "Vertex ID conversion error: " + std::string(e.what()),
            key_path
        };
    }

    return "\"" + id_str + "\"";
}

Result<std::string> StatementGenerator::format_value(const Value& value) {
    if (value.is_null) {
        return "NULL";
    }

    try {
        std::stringstream ss;
        std::visit([&](const auto& v) {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, std::string>) {
                ss << "\"" << v << "\"";
            }
            else if constexpr (std::is_same_v<T, bool>) {
                ss << (v ? "true" : "false");
            }
            else {
                ss << v;
            }
        }, value.value);

        return ss.str();
    }
    catch (const std::exception& e) {
        return StatementError{
            "Value formatting error: " + std::string(e.what())
        };
    }
}

std::string StatementGenerator::quote_identifier(const std::string& identifier) {
    // Check if identifier needs quoting
    bool needs_quotes = false;
    if (!identifier.empty()) {
        needs_quotes = !std::isalpha(identifier[0]) && identifier[0] != '_';
        if (!needs_quotes) {
            needs_quotes = !std::all_of(identifier.begin() + 1, identifier.end(),
                [](char c) { return std::isalnum(c) || c == '_'; });
        }
    }

    if (needs_quotes) {
        return "`" + identifier + "`";
    }
    return identifier;
}

namespace detail {
    std::string join_values(
        const std::vector<std::string>& values,
        const std::string& delimiter) {

        if (values.empty()) return "";

        std::stringstream ss;
        auto it = values.begin();
        ss << *it++;

        while (it != values.end()) {
            ss << delimiter << *it++;
        }

        return ss.str();
    }

    std::string build_property_list(
        const std::vector<std::pair<std::string, std::string>>& properties) {

        std::vector<std::string> parts;
        for (const auto& [name, value] : properties) {
            parts.push_back(name + "=" + value);
        }
        return join_values(parts, ", ");
    }
}

} // namespace graph