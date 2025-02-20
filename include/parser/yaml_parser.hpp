#ifndef NEBULA_MAPPER_YAML_PARSER_HPP
#define NEBULA_MAPPER_YAML_PARSER_HPP

#include "common/result.hpp"
#include <iostream>
#include <yaml-cpp/yaml.h>
#include <string>
#include <map>
#include <optional>

namespace parser::yaml {

    struct TransformRule {
        std::string name;
        std::string type;
        std::string condition;
        std::string value;     // For direct value mappings
        std::string field;     // For field extractions
        std::map<std::string, std::string> mappings;  // For value-to-property mappings
    };

    struct Transform {
        enum class Type {
            NONE,
            ARRAY_TO_BOOL,    // Convert array elements to boolean properties
            ARRAY_JOIN,       // Join array elements into string
            CUSTOM           // Custom transformation
        };

        Type type{Type::NONE};
        std::vector<TransformRule> rules;
        std::string join_delimiter{","};

        // Array transform specific fields
        std::string array_field;       // Field in array to check
        std::string array_condition;   // Condition to evaluate
        std::vector<std::pair<std::string, std::string>> mappings;  // value -> property mappings
    };

    struct DynamicFieldsConfig {
        bool enabled{false};
        std::set<std::string> allowed_types;
        std::set<std::string> excluded_properties;

        // Add conversion operator from bool
        DynamicFieldsConfig& operator=(bool value) {
            enabled = value;
            return *this;
        }
    };

    struct PropertyMapping {
        std::string json_path;
        std::string name;
        std::string nebula_type;
        bool optional{false};
        bool indexable{false};
        size_t max_length{256};
        std::optional<std::string> default_value;
        std::optional<Transform> transform;
    };

    struct TagMapping {
        std::string json_path;
        std::string key_field;
        std::map<std::string, PropertyMapping> properties;
        DynamicFieldsConfig dynamic_fields;
    };

    // YAML-specific error type
    struct Error : common::Error {
        std::optional<size_t> line{std::nullopt};
        std::optional<size_t> column{std::nullopt};

        Error(const std::string& msg,
              std::optional<size_t> l = std::nullopt,
              std::optional<size_t> col = std::nullopt)
            : common::Error(msg), line(l), column(col) {}
    };

    // Use common Result with YAML Error
    template<typename T>
    using Result = common::Result<T, Error>;


    // struct TagMapping {
    //     std::string json_path;
    //     std::string key_field;
    //     std::map<std::string, PropertyMapping> properties;
    //     bool allow_dynamic_fields{false};
    // };

    struct EdgeEndpoint {
        std::string tag;
        std::string key_field;
    };

    struct EdgeMapping {
        std::string json_path;
        EdgeEndpoint from;
        EdgeEndpoint to;
        std::map<std::string, PropertyMapping> properties;
    };



    // Parser functions
    Result<YAML::Node> parse(const std::string& content);
    Result<YAML::Node> parse_file(const std::string& file_path);

} // namespace parser::yaml

// YAML conversion specializations
namespace YAML {

    template<>
    struct convert<parser::yaml::TransformRule> {
        static bool decode(const Node& node, parser::yaml::TransformRule& rhs) {
            if (!node.IsMap()) {
                std::cerr << "Transform rule node is not a map" << std::endl;
                return false;
            }

            try {
                std::cerr << "Parsing transform rule with keys: ";
                for (const auto& key : node) {
                    std::cerr << key.first.as<std::string>() << " ";
                }
                std::cerr << std::endl;

                // Basic fields
                if (node["name"]) rhs.name = node["name"].as<std::string>();
                if (node["type"]) rhs.type = node["type"].as<std::string>();
                if (node["condition"]) rhs.condition = node["condition"].as<std::string>();
                if (node["value"]) rhs.value = node["value"].as<std::string>();
                if (node["field"]) rhs.field = node["field"].as<std::string>();

                // Handle mappings if present
                if (node["mappings"] && node["mappings"].IsMap()) {
                    for (const auto& mapping : node["mappings"]) {
                        rhs.mappings[mapping.first.as<std::string>()] =
                            mapping.second.as<std::string>();
                    }
                }

                // Debug output
                std::cerr << "Successfully parsed transform rule: " << rhs.name;
                if (!rhs.type.empty()) std::cerr << " (type: " << rhs.type << ")";
                if (!rhs.condition.empty()) std::cerr << " (condition: " << rhs.condition << ")";
                if (!rhs.mappings.empty()) std::cerr << " (with " << rhs.mappings.size() << " mappings)";
                std::cerr << std::endl;

                return true;
            } catch (const std::exception& e) {
                std::cerr << "Error parsing transform rule: " << e.what() << std::endl;
                return false;
            }
        }
    };

    template<>
    struct convert<parser::yaml::Transform> {
        static bool decode(const Node& node, parser::yaml::Transform& rhs) {
            if (!node.IsMap() && !node.IsSequence()) {
                std::cerr << "Transform node must be a map or sequence" << std::endl;
                return false;
            }

            try {
                if (node.IsMap()) {
                    // Parse transform type
                    if (node["type"]) {
                        std::string type_str = node["type"].as<std::string>();
                        if (type_str == "ARRAY_TO_BOOL") {
                            rhs.type = parser::yaml::Transform::Type::ARRAY_TO_BOOL;
                        } else if (type_str == "ARRAY_JOIN") {
                            rhs.type = parser::yaml::Transform::Type::ARRAY_JOIN;
                        } else {
                            rhs.type = parser::yaml::Transform::Type::CUSTOM;
                        }
                    }

                    // Parse array transform fields
                    if (node["field"]) {
                        rhs.array_field = node["field"].as<std::string>();
                    }
                    if (node["condition"]) {
                        rhs.array_condition = node["condition"].as<std::string>();
                    }
                    if (node["delimiter"]) {
                        rhs.join_delimiter = node["delimiter"].as<std::string>();
                    }

                    // Parse mappings if present
                    if (node["mappings"] && node["mappings"].IsMap()) {
                        for (const auto& mapping : node["mappings"]) {
                            rhs.mappings.push_back({
                                mapping.first.as<std::string>(),
                                mapping.second.as<std::string>()
                            });
                        }
                    }

                    // Parse transform rules
                    if (node["rules"] && node["rules"].IsSequence()) {
                        for (const auto& rule_node : node["rules"]) {
                            parser::yaml::TransformRule rule;
                            if (convert<parser::yaml::TransformRule>::decode(rule_node, rule)) {
                                rhs.rules.push_back(rule);
                            }
                        }
                    }
                } else {
                    // IsSequence - treat as a list of rules
                    rhs.type = parser::yaml::Transform::Type::CUSTOM;
                    for (const auto& rule_node : node) {
                        parser::yaml::TransformRule rule;
                        if (convert<parser::yaml::TransformRule>::decode(rule_node, rule)) {
                            rhs.rules.push_back(rule);
                        }
                    }
                }

                return true;
            } catch (const std::exception& e) {
                std::cerr << "Error parsing transform: " << e.what() << std::endl;
                return false;
            }
        }
    };

template<>
    struct convert<parser::yaml::PropertyMapping> {
        static bool decode(const Node& node, parser::yaml::PropertyMapping& rhs) {
            if (!node.IsMap()) {
                std::cerr << "Property node is not a map" << std::endl;
                return false;
            }

            try {
                std::cerr << "Parsing property with keys: ";
                for (const auto& key : node) {
                    std::cerr << key.first.as<std::string>() << " ";
                }
                std::cerr << std::endl;

                // Required field: json
                if (!node["json"]) {
                    std::cerr << "Missing 'json' field in property" << std::endl;
                    return false;
                }
                rhs.json_path = node["json"].as<std::string>();

                // Name field (optional)
                if (node["name"]) {
                    rhs.name = node["name"].as<std::string>();
                } else {
                    std::string prop_name = rhs.json_path;
                    std::replace(prop_name.begin(), prop_name.end(), '.', '_');
                    rhs.name = prop_name;
                }

                // Type field (required unless transform is present)
                if (node["type"]) {
                    rhs.nebula_type = node["type"].as<std::string>();
                }

                // Optional field
                if (node["optional"]) {
                    rhs.optional = node["optional"].as<bool>();
                } else {
                    rhs.optional = false;  // default to NOT NULL
                }

                // Indexable field
                if (node["index"]) {
                    rhs.indexable = node["index"].as<bool>();
                } else if (node["indexable"]) {
                    rhs.indexable = node["indexable"].as<bool>();
                } else {
                    rhs.indexable = false;  // default to not indexed
                }

                // Handle transforms
                if (node["transform"]) {
                    parser::yaml::Transform transform;

                    if (node["transform"].IsSequence()) {
                        // Array transforms
                        for (const auto& rule_node : node["transform"]) {
                            parser::yaml::TransformRule rule;
                            if (convert<parser::yaml::TransformRule>::decode(rule_node, rule)) {
                                transform.rules.push_back(rule);
                            }
                        }
                    } else if (node["transform"].IsMap()) {
                        // Handle join transform
                        transform.join_delimiter = node["transform"]["delimiter"] ?
                            node["transform"]["delimiter"].as<std::string>() : ",";
                    }

                    rhs.transform = transform;

                    // For transform properties, if type is not explicitly specified,
                    // use the type from the first transform rule or default to STRING
                    if (!node["type"] && !transform.rules.empty()) {
                        rhs.nebula_type = transform.rules[0].type;
                    }
                }

                // Type must be present either directly or through transform
                if (rhs.nebula_type.empty()) {
                    std::cerr << "Missing 'type' field in property" << std::endl;
                    return false;
                }

                if (node["max_length"]) {
                    rhs.max_length = node["max_length"].as<size_t>();
                }
                if (node["default"]) {
                    rhs.default_value = node["default"].as<std::string>();
                }

                std::cerr << "Successfully parsed property: " << rhs.name
                         << " of type " << rhs.nebula_type
                         << (rhs.optional ? " (optional)" : " (required)")
                         << (rhs.indexable ? " (indexed)" : "")
                         << std::endl;
                return true;
            } catch (const std::exception& e) {
                std::cerr << "Error parsing property: " << e.what() << std::endl;
                return false;
            }
        }
    };

    template<>
    struct convert<parser::yaml::EdgeEndpoint> {
        static bool decode(const Node& node, parser::yaml::EdgeEndpoint& rhs) {
            if (!node.IsMap()) return false;

            if (!node["tag"] || !node["key_field"]) return false;

            rhs.tag = node["tag"].as<std::string>();
            rhs.key_field = node["key_field"].as<std::string>();

            return true;
        }
    };

    template<>
    struct convert<parser::yaml::DynamicFieldsConfig> {
        static bool decode(const Node& node, parser::yaml::DynamicFieldsConfig& rhs) {
            if (!node.IsMap()) return false;

            // Check if dynamic fields are enabled
            if (node["enabled"]) {
                rhs.enabled = node["enabled"].as<bool>();
            }

            // Parse allowed types
            if (node["allowed_types"] && node["allowed_types"].IsSequence()) {
                for (const auto& type : node["allowed_types"]) {
                    rhs.allowed_types.insert(type.as<std::string>());
                }
            }

            // Parse excluded properties
            if (node["excluded_properties"] && node["excluded_properties"].IsSequence()) {
                for (const auto& prop : node["excluded_properties"]) {
                    rhs.excluded_properties.insert(prop.as<std::string>());
                }
            }

            return true;
        }
    };

    template<>
    struct convert<parser::yaml::TagMapping> {
        static bool decode(const Node& node, parser::yaml::TagMapping& rhs) {
            if (!node.IsMap()) return false;

            if (!node["from"]) return false;
            rhs.json_path = node["from"].as<std::string>();

            if (node["key"]) {
                rhs.key_field = node["key"].as<std::string>();
            } else {
                rhs.key_field = "id";  // Default key field
            }

            // Parse dynamic fields configuration
            if (node["dynamic_fields"]) {
                if (node["dynamic_fields"].IsMap()) {
                    rhs.dynamic_fields = node["dynamic_fields"].as<parser::yaml::DynamicFieldsConfig>();
                } else if (node["dynamic_fields"].IsScalar()) {
                    // Handle simple boolean case
                    rhs.dynamic_fields.enabled = node["dynamic_fields"].as<bool>();
                }
            }

            // Parse properties
            if (node["properties"] && node["properties"].IsSequence()) {
                for (const auto& prop : node["properties"]) {
                    parser::yaml::PropertyMapping prop_mapping;
                    if (convert<parser::yaml::PropertyMapping>::decode(prop, prop_mapping)) {
                        rhs.properties[prop_mapping.name] = prop_mapping;
                    }
                }
            }

            return true;
        }
    };

 template<>
    struct convert<parser::yaml::EdgeMapping> {
        static bool decode(const Node& node, parser::yaml::EdgeMapping& rhs) {
            if (!node.IsMap()) {
                std::cerr << "Node is not a map" << std::endl;
                return false;
            }

            try {
                // Debug output
                std::cerr << "Parsing edge mapping with keys: ";
                for (const auto& key : node) {
                    std::cerr << key.first.as<std::string>() << " ";
                }
                std::cerr << std::endl;

                // Required fields with new structure
                if (!node["from"]) {
                    std::cerr << "Missing 'from' field" << std::endl;
                    return false;
                }
                if (!node["source_tag"]) {
                    std::cerr << "Missing 'source_tag' field" << std::endl;
                    return false;
                }
                if (!node["target_tag"]) {
                    std::cerr << "Missing 'target_tag' field" << std::endl;
                    return false;
                }

                rhs.json_path = node["from"].as<std::string>();

                // Set up endpoints
                rhs.from.tag = node["source_tag"].as<std::string>();
                rhs.from.key_field = "id";  // Default key field
                rhs.to.tag = node["target_tag"].as<std::string>();
                rhs.to.key_field = "id";  // Default key field

                // Handle properties
                if (node["properties"] && node["properties"].IsSequence()) {
                    std::cerr << "Processing properties" << std::endl;
                    for (const auto& prop : node["properties"]) {
                        auto prop_mapping = prop.as<parser::yaml::PropertyMapping>();
                        rhs.properties[prop_mapping.name] = prop_mapping;
                    }
                }

                return true;
            }
            catch (const YAML::Exception& e) {
                std::cerr << "YAML parsing error: " << e.what() << std::endl;
                return false;
            }
            catch (const std::exception& e) {
                std::cerr << "Error parsing edge mapping: " << e.what() << std::endl;
                return false;
            }
        }
    };

} // namespace YAML

#endif // NEBULA_MAPPER_YAML_PARSER_HPP