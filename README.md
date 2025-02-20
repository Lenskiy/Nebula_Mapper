# Nebula Mapper

A C++ library for mapping JSON data to NebulaGraph database using YAML configurations. This tool helps automate the process of importing JSON data into NebulaGraph by providing a flexible and declarative mapping system.

## Features

- Declarative YAML-based mapping configuration
- JSON path-based data extraction
- Support for multiple data types and transformations
- Automatic schema generation for NebulaGraph
- Dynamic field support
- Index management
- Property transformations and validations
- Support for composite keys
- Batch processing capabilities

## Prerequisites

- C++17 or later
- CMake 3.14 or later
- yaml-cpp
- nlohmann/json
- NebulaGraph client library

## Building

```bash
mkdir build
cd build
cmake ..
make
```

## Usage

1. Create a YAML mapping file that defines how your JSON data maps to NebulaGraph vertices and edges:

```yaml
tags:
  Place:
    from: basicInfo
    key: cid    
    properties:
      - json: cid
        type: INT
        index: true
      - json: placenamefull
        type: STRING
        index: true
      - json: wpointx
        type: INT

edges:
  Comment:
    from: comment.list[*]
    source_tag: User
    target_tag: Place
    source_key: kakaoMapUserId
    target_key: cid
    properties:
      - json: commentid
        type: STRING
        index: true
```

2. Use the library to process your JSON data:

```cpp
#include "parser/mapping_parser.hpp"
#include "graph/statement_generator.hpp"

int main() {
    // Load mapping configuration
    auto config = parser::yaml::parse_file("mapping.yaml");
    auto mapping = parser::mapping::create_mapping(config);

    // Load JSON data
    auto json_data = parser::json::parse_file("data.json");

    // Generate statements
    graph::StatementGenerator generator;
    auto statements = generator.generate_batch_statements(
        std::get<parser::mapping::GraphMapping>(mapping),
        std::get<parser::json::JsonDocument>(json_data)
    );
}
```

## Configuration Guide

### Tags (Vertices)

- `from`: JSON path to source data
- `key`: Field to use as vertex ID
- `properties`: List of property mappings
  - `json`: JSON path to property value
  - `type`: Data type (INT, STRING, BOOL, etc.)
  - `index`: Whether to create an index
  - `optional`: Whether the property is required

### Edges

- `from`: JSON path to edge data
- `source_tag`/`target_tag`: Vertex types to connect
- `source_key`/`target_key`: Keys for vertex identification
- `properties`: Edge property mappings

### Property Transformations

```yaml
properties:
  - json: strengths
    transform:
      type: CUSTOM
      rules:
        - name: strength_taste
          type: BOOL
          condition: "id=5 AND name='맛'"
```

## Error Handling

The library uses a Result type for error handling:

```cpp
auto result = generator.generate_statements(mapping, json_data);
if (std::holds_alternative<Error>(result)) {
    auto error = std::get<Error>(result);
    std::cerr << "Error: " << error.message << std::endl;
    return 1;
}
```

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

## Acknowledgments

This project uses:
- [yaml-cpp](https://github.com/jbeder/yaml-cpp) for YAML parsing
- [nlohmann/json](https://github.com/nlohmann/json) for JSON handling
