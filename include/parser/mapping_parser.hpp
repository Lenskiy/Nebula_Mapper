#ifndef NEBULA_MAPPER_MAPPING_PARSER_HPP
#define NEBULA_MAPPER_MAPPING_PARSER_HPP

#include "common/result.hpp"
#include "json_parser.hpp"
#include "yaml_parser.hpp"

namespace parser::mapping {

// Mapping-specific error type
struct Error : common::Error {
    Error(const std::string& msg,
          const std::optional<std::string>& ctx = std::nullopt)
        : common::Error(msg, ctx) {}
};

// Use common Result with Mapping Error
template<typename T>
using Result = common::Result<T, Error>;

// Transform type for data conversion
struct Transform {
    std::string type;
    std::map<std::string, std::string> params;
};

// Property in the final mapping
    struct Property {
        std::string name;
        std::string json_path;
        std::string nebula_type;
        bool optional{false};
        bool indexable{false};  // Add this line
        std::optional<std::string> default_value;
        std::optional<Transform> transform;
    };

    using DynamicFieldsConfig = yaml::DynamicFieldsConfig;

    struct VertexMapping {
        std::string tag_name;
        std::string source_path;
        std::string key_path;
        std::vector<Property> properties;
        DynamicFieldsConfig dynamic_fields;  // Changed from bool to DynamicFieldsConfig
    };

// Edge mapping
struct EdgeMapping {
    std::string edge_name;
    std::string source_path;
    struct {
        std::string tag;
        std::string key_path;
    } from;
    struct {
        std::string tag;
        std::string key_path;
    } to;
    std::vector<Property> properties;
};

// Complete graph mapping
struct GraphMapping {
    std::vector<VertexMapping> vertices;
    std::vector<EdgeMapping> edges;
    std::map<std::string, Transform> transforms;

    struct {
        size_t string_length{256};
        std::string array_delimiter{","};
        bool allow_dynamic_tags{false};
    } settings;
};

// Main mapping creation function
Result<parser::mapping::GraphMapping> create_mapping(const parser::yaml::Result<YAML::Node>& config);

// Validation function
Result<void> validate_mapping(const parser::mapping::GraphMapping& mapping,
                            const parser::json::JsonDocument& document);

namespace detail {
    Result<VertexMapping> create_vertex_mapping(
        const parser::yaml::TagMapping& tag_def,
        const std::string& tag_name);

    Result<EdgeMapping> create_edge_mapping(
        const parser::yaml::EdgeMapping& edge_def,
        const std::string& edge_name);

    Result<Property> create_property_mapping(
        const parser::yaml::PropertyMapping& prop_def,
        const std::string& prop_name);
}

} // namespace parser::mapping

#endif