#ifndef NEBULA_MAPPER_MAPPING_VALIDATION_HPP
#define NEBULA_MAPPER_MAPPING_VALIDATION_HPP

#include "parser/mapping_parser.hpp"
#include <string>
#include <vector>
#include <functional>

namespace parser::mapping::validation {

struct ValidationContext {
    std::string element_name;
    std::string element_type;  // "vertex" or "edge"
    std::string source_path;

    ValidationContext(const std::string& name, const std::string& type, const std::string& path)
        : element_name(name), element_type(type), source_path(path) {}
};

// Property validation result
struct PropertyValidation {
    bool is_valid;
    std::string error_message;

    static PropertyValidation success() {
        return {true, ""};
    }

    static PropertyValidation failure(const std::string& message) {
        return {false, message};
    }
};

// Validator for property mappings
class PropertyValidator {
public:
    // Validate individual property
    static PropertyValidation validate_property(
        const PropertyMapping& prop,
        const ValidationContext& context);

    // Validate a collection of properties
    static Result<void> validate_properties(
        const std::vector<PropertyMapping>& properties,
        const ValidationContext& context);

private:
    static PropertyValidation validate_property_name(const std::string& name);
    static PropertyValidation validate_property_type(const std::string& type);
    static PropertyValidation validate_property_path(const std::string& path);
};

// Common mapping validation functions
class MappingValidator {
public:
    // Validate source path
    static Result<void> validate_source_path(
        const std::string& path,
        const ValidationContext& context);

    // Validate key field
    static Result<void> validate_key_field(
        const std::string& key_field,
        const ValidationContext& context);

    // Validate dynamic fields configuration
    static Result<void> validate_dynamic_fields(
        const DynamicFieldsConfig& config,
        const ValidationContext& context);

    // Validate edge endpoints
    static Result<void> validate_edge_endpoints(
        const EdgeEndpoint& from,
        const EdgeEndpoint& to,
        const ValidationContext& context);

    // Common validation logic shared between vertex and edge mappings
    template<typename T>
    static Result<void> validate_common(const T& mapping, const ValidationContext& context) {
        // Validate source path
        auto path_result = validate_source_path(mapping.source_path, context);
        if (std::holds_alternative<Error>(path_result)) {
            return std::get<Error>(path_result);
        }

        // Validate key field
        auto key_result = validate_key_field(mapping.key_path, context);
        if (std::holds_alternative<Error>(key_result)) {
            return std::get<Error>(key_result);
        }

        // Validate properties
        auto prop_result = PropertyValidator::validate_properties(
            mapping.properties,
            context
        );
        if (std::holds_alternative<Error>(prop_result)) {
            return std::get<Error>(prop_result);
        }

        return {};
    }

private:
    static bool is_valid_identifier(const std::string& str);
    static bool is_valid_json_path(const std::string& path);
};

} // namespace parser::mapping::validation

#endif // NEBULA_MAPPER_MAPPING_VALIDATION_HPP