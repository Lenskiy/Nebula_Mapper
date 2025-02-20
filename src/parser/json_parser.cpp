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

Result<JsonDocument> JsonParser::parse(const std::string& input) {
    try {
        return JsonDocument::parse(input);
    } catch (const JsonDocument::exception& e) {
        return Error{e.what()};
    }
}

Result<JsonDocument> JsonParser::parse_file(const std::string& file_path) {
    try {
        return parse(read_file(file_path));
    } catch (const std::exception& e) {
        return Error{"File error: " + std::string(e.what())};
    }
}

const std::vector<std::string>& JsonParser::get_parsed_path(const std::string& path) {
    // First check if path is already cached (read-only)
    {
        std::shared_lock read_lock(cache_mutex_);
        auto it = path_cache_.find(path);
        if (it != path_cache_.end()) {
            return it->second;
        }
    }

    // If not in cache, parse and cache it (write lock)
    {
        std::unique_lock write_lock(cache_mutex_);
        // Double-check in case another thread cached it while we were waiting
        auto it = path_cache_.find(path);
        if (it != path_cache_.end()) {
            return it->second;
        }

        // Parse and cache the path
        auto segments = common::utils::split_path(path);
        auto [cache_it, _] = path_cache_.emplace(path, std::move(segments));
        return cache_it->second;
    }
}

Result<JsonDocument> JsonParser::navigate_path(
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

bool JsonParser::has_path(const JsonDocument& j, const std::string& path) {
    const auto& segments = get_parsed_path(path);
    auto result = navigate_path(j, segments);
    return std::holds_alternative<JsonDocument>(result);
}

Result<std::string> JsonParser::to_string(const JsonDocument& j) {
    try {
        return j.dump();
    } catch (const JsonDocument::exception& e) {
        return Error{"Failed to convert JSON to string: " + std::string(e.what())};
    }
}

void JsonParser::clear_cache() {
    std::unique_lock lock(cache_mutex_);
    path_cache_.clear();
}

size_t JsonParser::cache_size() const {
    std::shared_lock lock(cache_mutex_);
    return path_cache_.size();
}

} // namespace parser::json