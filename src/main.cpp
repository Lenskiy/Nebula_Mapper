#include <iostream>
#include <filesystem>
#include <fstream>
#include <sstream>
#include "parser/json_parser.hpp"
#include "parser/yaml_parser.hpp"
#include "parser/mapping_parser.hpp"
#include "graph/schema_manager.hpp"
#include "graph/statement_generator.hpp"

namespace fs = std::filesystem;

void print_usage(const char* program_name) {
    std::cerr << "Usage: " << program_name
              << " <mapping.yaml> <input.json> [--schema-only] [--batch-size N]\n"
              << "Options:\n"
              << "  --schema-only  Only generate schema statements\n"
              << "  --batch-size N Batch size for INSERT statements (default: 500)\n";
}

std::optional<std::string> read_file(const fs::path& path) {
    try {
        std::ifstream file(path);
        if (!file) {
            std::cerr << "Error: Cannot open file: " << path << '\n';
            return std::nullopt;
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    } catch (const std::exception& e) {
        std::cerr << "Error reading file: " << path << " - " << e.what() << '\n';
        return std::nullopt;
    }
}

struct ProgramOptions {
    fs::path mapping_file;
    fs::path input_file;
    bool schema_only{false};
    size_t batch_size{500};
};

std::optional<ProgramOptions> parse_arguments(int argc, char* argv[]) {
    if (argc < 3) {
        print_usage(argv[0]);
        return std::nullopt;
    }

    ProgramOptions options;
    options.mapping_file = argv[1];
    options.input_file = argv[2];

    for (int i = 3; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--schema-only") {
            options.schema_only = true;
        } else if (arg == "--batch-size" && i + 1 < argc) {
            try {
                options.batch_size = std::stoul(argv[++i]);
            } catch (const std::exception&) {
                std::cerr << "Error: Invalid batch size\n";
                return std::nullopt;
            }
        } else {
            std::cerr << "Error: Unknown option: " << arg << '\n';
            print_usage(argv[0]);
            return std::nullopt;
        }
    }

    return options;
}

template<typename T>
void print_error(const T& error) {
    if constexpr (std::is_same_v<T, parser::json::Error>) {
        std::cerr << "JSON Error: " << error.message;
        if (error.line_number) {
            std::cerr << " at line " << *error.line_number;
        }
        std::cerr << '\n';
    }
    else if constexpr (std::is_same_v<T, parser::yaml::Error>) {
        std::cerr << "YAML Error: " << error.message;
        if (error.line) {
            std::cerr << " at line " << *error.line;
        }
        std::cerr << '\n';
    }
    else if constexpr (std::is_same_v<T, parser::mapping::Error>) {
        std::cerr << "Mapping Error: " << error.message;
        if (error.context) {
            std::cerr << " (" << *error.context << ")";
        }
        std::cerr << '\n';
    }
    else if constexpr (std::is_same_v<T, graph::SchemaError>) {
        std::cerr << "Schema Error: " << error.message;
        if (error.context) {
            std::cerr << " in " << *error.context;
        }
        std::cerr << '\n';
    }
    else {
        std::cerr << "Error: " << error.message << '\n';
    }
}

int main(int argc, char* argv[]) {
    try {
        // Parse command line arguments
        auto options = parse_arguments(argc, argv);
        if (!options) {
            return 1;
        }

        // Read input files
        auto yaml_content = read_file(options->mapping_file);
        auto json_content = read_file(options->input_file);
        if (!yaml_content || !json_content) {
            return 1;
        }

        // Parse YAML mapping
        auto yaml_result = parser::yaml::parse(*yaml_content);
        if (std::holds_alternative<parser::yaml::Error>(yaml_result)) {
            print_error(std::get<parser::yaml::Error>(yaml_result));
            return 1;
        }

        // Parse JSON input
        auto json_result = parser::json::parse(*json_content);
        if (std::holds_alternative<parser::json::Error>(json_result)) {
            print_error(std::get<parser::json::Error>(json_result));
            return 1;
        }

        // Create mapping
        auto mapping_result = parser::mapping::create_mapping(yaml_result);
        if (std::holds_alternative<parser::mapping::Error>(mapping_result)) {
            print_error(std::get<parser::mapping::Error>(mapping_result));
            return 1;
        }

        // Generate schema statements
        graph::SchemaManager schema_manager;
        auto schema_result = schema_manager.generate_schema_statements(
            std::get<parser::mapping::GraphMapping>(mapping_result));

        if (std::holds_alternative<graph::SchemaError>(schema_result)) {
            print_error(std::get<graph::SchemaError>(schema_result));
            return 1;
        }

        // Print schema statements
        for (const auto& stmt : std::get<std::vector<std::string>>(schema_result)) {
            std::cout << stmt << "\n";
        }

        if (!options->schema_only) {
            // Generate insert statements
            graph::StatementGenerator stmt_generator;
            auto stmt_result = stmt_generator.generate_batch_statements(
                std::get<parser::mapping::GraphMapping>(mapping_result),
                std::get<parser::json::JsonDocument>(json_result),
                options->batch_size);

            if (std::holds_alternative<graph::StatementError>(stmt_result)) {
                print_error(std::get<graph::StatementError>(stmt_result));
                return 1;
            }

            // Print insert statements
            for (const auto& stmt : std::get<std::vector<std::string>>(stmt_result)) {
                std::cout << stmt << "\n";
            }
        }

        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "Unexpected error: " << e.what() << '\n';
        return 1;
    }
}