#include "parser/validation/mapping_validation.hpp"
#include "parser/yaml_parser.hpp"
#include <regex>

namespace parser::mapping::validation {

PropertyValidation PropertyValidator::validate_property(
    const PropertyMapping& prop,
    [[maybe_unused]] const ValidationContext& context) {

    // Validate property name
    auto name_validation = validate_property_name(prop.name);
    if (!name_validation.is_valid) {
        return name_validation;
    }

    // Validate property type
    auto type_validation = validate_property_type(prop.nebula_type);
    if (!type_validation.is_valid) {
        return type_validation;
    }

    // Validate JSON path
    auto path_validation = validate_property_path(prop.json_path);
    if (!path_validation.is_valid) {
        return path_validation;
    }

    return PropertyValidation::success();
}

Result<Success> PropertyValidator::validate_properties(
    const std::vector<PropertyMapping>& properties,
    const ValidationContext& context) {

    std::unordered_set<std::string> property_names;

    for (const auto& prop : properties) {
        // Check for duplicate property names
        if (property_names.find(prop.name) != property_names.end()) {
            return Error{
                "Duplicate property name: " + prop.name,
                context.element_name
            };
        }
        property_names.insert(prop.name);

        // Validate individual property
        auto validation = validate_property(prop, context);
        if (!validation.is_valid) {
            return Error{
                validation.error_message,
                context.element_name + "." + prop.name
            };
        }
    }

    return Success{};
}

Result<Success> MappingValidator::validate_source_path(
    const std::string& path,
    const ValidationContext& context) {

    if (path.empty()) {
        return Error{
            "Source path cannot be empty",
            context.element_name
        };
    }

    if (!is_valid_json_path(path)) {
        return Error{
            "Invalid source path: " + path,
            context.element_name
        };
    }

    return Success{};
}

Result<Success> MappingValidator::validate_key_field(
    const std::string& key_field,
    const ValidationContext& context) {

    if (key_field.empty()) {
        return Error{
            "Key field cannot be empty",
            context.element_name
        };
    }

    if (!is_valid_identifier(key_field)) {
        return Error{
            "Invalid key field identifier: " + key_field,
            context.element_name
        };
    }

    return Success{};
}

Result<Success> MappingValidator::validate_dynamic_fields(
    const DynamicFieldsConfig& config,
    const ValidationContext& context) {

    if (config.enabled) {
        // Validate allowed types if specified
        if (!config.allowed_types.empty()) {
            for (const auto& type : config.allowed_types) {
                auto validation = PropertyValidator::validate_property_type(type);
                if (!validation.is_valid) {
                    return Error{
                        "Invalid dynamic field type: " + type,
                        context.element_name
                    };
                }
            }
        }

        // Validate excluded properties if specified
        if (!config.excluded_properties.empty()) {
            for (const auto& prop : config.excluded_properties) {
                if (!is_valid_identifier(prop)) {
                    return Error{
                        "Invalid excluded property name: " + prop,
                        context.element_name
                    };
                }
            }
        }
    }

    return Success{};
}

Result<Success> MappingValidator::validate_edge_endpoints(
    const EdgeEndpoint& from,
    const EdgeEndpoint& to,
    const ValidationContext& context) {

    // Validate source endpoint
    if (from.tag.empty()) {
        return Error{
            "Source tag cannot be empty",
            context.element_name
        };
    }

    if (!is_valid_identifier(from.tag)) {
        return Error{
            "Invalid source tag identifier: " + from.tag,
            context.element_name
        };
    }

    // Validate target endpoint
    if (to.tag.empty()) {
        return Error{
            "Target tag cannot be empty",
            context.element_name
        };
    }

    if (!is_valid_identifier(to.tag)) {
        return Error{
            "Invalid target tag identifier: " + to.tag,
            context.element_name
        };
    }

    return Success{};
}

// Helper function implementations
PropertyValidation PropertyValidator::validate_property_name(const std::string& name) {
    static const std::regex name_regex("^[a-zA-Z_][a-zA-Z0-9_]*$");
    if (!std::regex_match(name, name_regex)) {
        return PropertyValidation::failure(
            "Invalid property name: " + name +
            ". Must start with letter or underscore and contain only alphanumeric characters."
        );
    }
    return PropertyValidation::success();
}

PropertyValidation PropertyValidator::validate_property_type(const std::string& type) {
    static const std::unordered_set<std::string> valid_types = {
        "BOOL", "INT", "FLOAT", "DOUBLE", "STRING",
        "DATE", "TIME", "DATETIME", "TIMESTAMP"
    };

    if (valid_types.find(type) == valid_types.end()) {
        return PropertyValidation::failure(
            "Invalid property type: " + type +
            ". Must be one of the valid Nebula Graph types."
        );
    }
    return PropertyValidation::success();
}

PropertyValidation PropertyValidator::validate_property_path(const std::string& path) {
    if (path.empty()) {
        return PropertyValidation::failure("Property path cannot be empty");
    }
    return PropertyValidation::success();
}

bool MappingValidator::is_valid_identifier(const std::string& str) {
    static const std::regex identifier_regex("^[a-zA-Z_][a-zA-Z0-9_]*$");
    return std::regex_match(str, identifier_regex);
}

bool MappingValidator::is_valid_json_path(const std::string& path) {
    if (path.empty() || path[0] != '/') {
        return false;
    }

    int bracket_count = 0;
    for (char c : path) {
        if (c == '[') bracket_count++;
        else if (c == ']') bracket_count--;
        if (bracket_count < 0) return false;
    }
    return bracket_count == 0;
}

} // namespace parser::mapping::validation