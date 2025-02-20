#include "parser/yaml_parser.hpp"
#include <fstream>
#include <sstream>

namespace parser::yaml {


    Result<YAML::Node> parse(const std::string& content) {
        try {
            YAML::Node config = YAML::Load(content);
            return config;
        } catch (const YAML::Exception& e) {
            return Error{
                "Failed to parse YAML content: " + std::string(e.what()),
                e.mark.line,
                e.mark.column
            };
        }
    }

    Result<YAML::Node> parse_file(const std::string& file_path) {
        try {
            YAML::Node config = YAML::LoadFile(file_path);
            return config;
        } catch (const YAML::Exception& e) {
            return Error{
                "Failed to load YAML file: " + std::string(e.what()),
                e.mark.line,
                e.mark.column
            };
        }
    }

namespace detail {

Result<PropertyMapping> parse_property(const YAML::Node& node) {
    if (!node.IsMap()) {
        return Error{"Property definition must be a mapping"};
    }

    PropertyMapping prop;

    // Parse required fields
    if (!node["json_path"] || !node["nebula_type"]) {
        return Error{"Property must have 'json_path' and 'nebula_type'"};
    }

    prop.json_path = node["json_path"].as<std::string>();
    prop.nebula_type = node["nebula_type"].as<std::string>();

    // Parse optional fields
    if (node["optional"]) {
        prop.optional = node["optional"].as<bool>();
    }

    if (node["max_length"]) {
        prop.max_length = node["max_length"].as<size_t>();
    }

    if (node["default"]) {
        prop.default_value = node["default"].as<std::string>();
    }

    return prop;
}

Result<TagMapping> parse_tag(const YAML::Node& node) {
    if (!node.IsMap()) {
        return Error{"Tag definition must be a mapping"};
    }

    if (!node["json_path"] || !node["key_field"]) {
        return Error{"Tag must have 'json_path' and 'key_field'"};
    }

    TagMapping tag;
    tag.json_path = node["json_path"].as<std::string>();
    tag.key_field = node["key_field"].as<std::string>();

    if (node["allow_dynamic_fields"]) {
        tag.dynamic_fields = node["allow_dynamic_fields"].as<bool>();
    }

    if (!node["properties"] || !node["properties"].IsMap()) {
        return Error{"Tag must have 'properties' mapping"};
    }

    for (const auto& prop : node["properties"]) {
        auto prop_result = parse_property(prop.second);
        if (std::holds_alternative<Error>(prop_result)) {
            return std::get<Error>(prop_result);
        }
        tag.properties[prop.first.as<std::string>()] =
            std::get<PropertyMapping>(prop_result);
    }

    return tag;
}

Result<EdgeEndpoint> parse_endpoint(const YAML::Node& node) {
    if (!node.IsMap()) {
        return Error{"Edge endpoint must be a mapping"};
    }

    if (!node["tag"] || !node["key_field"]) {
        return Error{"Edge endpoint must have 'tag' and 'key_field'"};
    }

    EdgeEndpoint endpoint;
    endpoint.tag = node["tag"].as<std::string>();
    endpoint.key_field = node["key_field"].as<std::string>();

    return endpoint;
}

Result<EdgeMapping> parse_edge(const YAML::Node& node) {
    if (!node.IsMap()) {
        return Error{"Edge definition must be a mapping"};
    }

    if (!node["json_path"] || !node["from"] || !node["to"]) {
        return Error{"Edge must have 'json_path', 'from', and 'to'"};
    }

    EdgeMapping edge;
    edge.json_path = node["json_path"].as<std::string>();

    auto from_result = parse_endpoint(node["from"]);
    if (std::holds_alternative<Error>(from_result)) {
        return std::get<Error>(from_result);
    }
    edge.from = std::get<EdgeEndpoint>(from_result);

    auto to_result = parse_endpoint(node["to"]);
    if (std::holds_alternative<Error>(to_result)) {
        return std::get<Error>(to_result);
    }
    edge.to = std::get<EdgeEndpoint>(to_result);

    if (node["properties"] && node["properties"].IsMap()) {
        for (const auto& prop : node["properties"]) {
            auto prop_result = parse_property(prop.second);
            if (std::holds_alternative<Error>(prop_result)) {
                return std::get<Error>(prop_result);
            }
            edge.properties[prop.first.as<std::string>()] =
                std::get<PropertyMapping>(prop_result);
        }
    }

    return edge;
}

} // namespace detail

} // namespace parser::yaml