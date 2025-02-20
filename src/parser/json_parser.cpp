#include "parser/json_parser.hpp"
#include <fstream>
#include <sstream>

namespace parser::json {

namespace {
    // Helper function to read file
    std::string read_file(const std::string& file_path) {
        std::ifstream file(file_path);
        if (!file) {
            throw std::runtime_error("Cannot open file: " + file_path);
        }
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }
}

Result<JsonDocument> parse(const std::string& input) {
    try {
        return JsonDocument::parse(input);
    } catch (const JsonDocument::exception& e) {
        return Error{e.what()};
    }
}

Result<JsonDocument> parse_file(const std::string& file_path) {
    try {
        return parse(read_file(file_path));
    } catch (const std::exception& e) {
        return Error{"File error: " + std::string(e.what())};
    }
}

bool has_path(const JsonDocument& j, const std::string& path) {
    auto result = detail::navigate_path(j, detail::split_path(path));
    return std::holds_alternative<JsonDocument>(result);
}

Result<std::string> to_string(const JsonDocument& j) {
    try {
        return j.dump();
    } catch (const JsonDocument::exception& e) {
        return Error{"Failed to convert JSON to string: " + std::string(e.what())};
    }
}

namespace detail {

std::vector<std::string> split_path(const std::string& path) {
    std::vector<std::string> segments;
    std::stringstream ss(path);
    std::string segment;

    // Skip leading slash if present
    if (!path.empty() && path[0] == '/') {
        ss.ignore();
    }

    // Split on '/' character
    while (std::getline(ss, segment, '/')) {
        if (!segment.empty()) {
            segments.push_back(segment);
        }
    }

    return segments;
}

Result<JsonDocument> navigate_path(
    const JsonDocument& j,
    const std::vector<std::string>& segments) {

    const JsonDocument* current = &j;

    for (const auto& segment : segments) {
        // Handle array indexing with [n] notation
        if (!segment.empty() && segment[0] == '[' && segment.back() == ']') {
            try {
                size_t index = std::stoul(segment.substr(1, segment.length() - 2));
                if (!current->is_array()) {
                    return Error{"Expected array at path segment: " + segment};
                }
                if (index >= current->size()) {
                    return Error{"Array index out of bounds: " + segment};
                }
                current = &(*current)[index];
            } catch (const std::exception& e) {
                return Error{"Invalid array index: " + segment};
            }
            continue;
        }

        // Handle object property access
        if (!current->is_object()) {
            return Error{"Expected object at path segment: " + segment};
        }

        auto it = current->find(segment);
        if (it == current->end()) {
            return Error{"Property not found: " + segment};
        }
        current = &(*it);
    }

    return *current;
}

} // namespace detail
} // namespace parser::json