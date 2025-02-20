#ifndef NEBULA_MAPPER_DYNAMIC_FIELDS_UTILS_HPP
#define NEBULA_MAPPER_DYNAMIC_FIELDS_UTILS_HPP

#include "yaml_parser.hpp"
#include "common/yaml_error_utils.hpp"
#include <yaml-cpp/yaml.h>
#include <set>
#include <string>

namespace parser::yaml::detail {

inline bool parse_dynamic_fields_bool(const YAML::Node& node, DynamicFieldsConfig& config) {
    config.enabled = node.as<bool>();
    return true;
}

inline bool parse_dynamic_fields_map(const YAML::Node& node, DynamicFieldsConfig& config) {
    if (!common::yaml::validate_node_type(node, YAML::NodeType::Map, "DynamicFieldsConfig")) {
        return false;
    }

    return common::yaml::handle_yaml_error<DynamicFieldsConfig>(
        "DynamicFieldsConfig",
        [&](DynamicFieldsConfig& cfg) {
            // Parse enabled flag
            if (node["enabled"]) {
                cfg.enabled = node["enabled"].as<bool>();
            }

            // Parse allowed types
            if (node["allowed_types"] && node["allowed_types"].IsSequence()) {
                for (const auto& type : node["allowed_types"]) {
                    cfg.allowed_types.insert(type.as<std::string>());
                }
            }

            // Parse excluded properties
            if (node["excluded_properties"] && node["excluded_properties"].IsSequence()) {
                for (const auto& prop : node["excluded_properties"]) {
                    cfg.excluded_properties.insert(prop.as<std::string>());
                }
            }
            return true;
        },
        config
    );
}

inline bool parse_dynamic_fields(const YAML::Node& node, DynamicFieldsConfig& config) {
    if (node.IsScalar()) {
        return parse_dynamic_fields_bool(node, config);
    } else if (node.IsMap()) {
        return parse_dynamic_fields_map(node, config);
    }

    std::cerr << "DynamicFieldsConfig must be either a boolean or a map" << std::endl;
    return false;
}

} // namespace parser::yaml::detail

#endif // NEBULA_MAPPER_DYNAMIC_FIELDS_UTILS_HPP