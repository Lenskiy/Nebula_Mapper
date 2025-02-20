// common/string_utils.hpp
#ifndef NEBULA_MAPPER_STRING_UTILS_HPP
#define NEBULA_MAPPER_STRING_UTILS_HPP

#include <string>
#include <vector>
#include <sstream>

namespace common::utils {

// Splits a string by delimiter, optionally skipping empty parts
inline std::vector<std::string> split_string(
    const std::string& str,
    char delimiter,
    bool skip_empty = false) {

    std::vector<std::string> parts;
    std::stringstream ss(str);
    std::string part;

    while (std::getline(ss, part, delimiter)) {
        if (!skip_empty || !part.empty()) {
            parts.push_back(std::move(part));
        }
    }

    return parts;
}

// Splits a path string into segments, handling special cases for JSON paths
inline std::vector<std::string> split_path(const std::string& path) {
    std::vector<std::string> segments;

    if (path.empty()) {
        return segments;
    }

    // Skip leading slash if present
    size_t start = (path[0] == '/') ? 1 : 0;
    size_t pos = start;

    while (pos < path.length()) {
        // Handle array indexing with [n] notation
        if (path[pos] == '[') {
            size_t end = path.find(']', pos);
            if (end != std::string::npos) {
                if (pos > start) {
                    segments.push_back(path.substr(start, pos - start));
                }
                segments.push_back(path.substr(pos, end - pos + 1));
                pos = end + 1;
                start = pos;
                // Skip the following slash if present
                if (pos < path.length() && path[pos] == '/') {
                    start = ++pos;
                }
                continue;
            }
        }

        // Handle normal path segments
        if (path[pos] == '/') {
            if (pos > start) {
                segments.push_back(path.substr(start, pos - start));
            }
            start = pos + 1;
        }
        pos++;
    }

    // Add the last segment if any
    if (start < path.length()) {
        segments.push_back(path.substr(start));
    }

    return segments;
}

} // namespace common::utils

#endif // NEBULA_MAPPER_STRING_UTILS_HPP