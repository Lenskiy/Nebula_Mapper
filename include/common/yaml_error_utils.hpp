#ifndef NEBULA_MAPPER_YAML_ERROR_UTILS_HPP
#define NEBULA_MAPPER_YAML_ERROR_UTILS_HPP

#include <yaml-cpp/yaml.h>
#include <iostream>
#include <functional>

namespace common::yaml {

// Error logging utility
template<typename T>
bool handle_yaml_error(const std::string& context, std::function<bool(T&)> parser_fn, T& target) {
    try {
        return parser_fn(target);
    } catch (const YAML::Exception& e) {
        std::cerr << "YAML Error parsing " << context << ": " << e.what() << std::endl;
        return false;
    } catch (const std::exception& e) {
        std::cerr << "Error parsing " << context << ": " << e.what() << std::endl;
        return false;
    }
}

// Debug logging utility for node contents
inline void log_node_keys(const YAML::Node& node, const std::string& context = "") {
    if (!node.IsMap()) {
        std::cerr << context << " node is not a map" << std::endl;
        return;
    }

    std::cerr << "Parsing " << context << " with keys: ";
    for (const auto& key : node) {
        std::cerr << key.first.as<std::string>() << " ";
    }
    std::cerr << std::endl;
}

// Validation utilities
inline bool validate_required_fields(const YAML::Node& node,
                                  const std::vector<std::string>& required_fields,
                                  const std::string& context) {
    for (const auto& field : required_fields) {
        if (!node[field]) {
            std::cerr << context << " must have '" << field << "' field" << std::endl;
            return false;
        }
    }
    return true;
}

// Type checking utilities
inline bool validate_node_type(const YAML::Node& node,
                             YAML::NodeType::value expected_type,
                             const std::string& context) {
    if (node.Type() != expected_type) {
        std::cerr << context << " must be a "
                 << (expected_type == YAML::NodeType::Map ? "map" :
                     expected_type == YAML::NodeType::Sequence ? "sequence" : "scalar")
                 << std::endl;
        return false;
    }
    return true;
}

} // namespace common::yaml

#endif // NEBULA_MAPPER_YAML_ERROR_UTILS_HPP