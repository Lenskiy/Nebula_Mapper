#ifndef NEBULA_MAPPER_MAPPING_ERRORS_HPP
#define NEBULA_MAPPER_MAPPING_ERRORS_HPP

#include "common/result.hpp"
#include <string>
#include <optional>

namespace parser::mapping {

    // Define the base error type for mapping operations
    struct Error : common::Error {
        Error(const std::string& msg,
              const std::optional<std::string>& ctx = std::nullopt)
            : common::Error(msg, ctx) {}
    };

    // Template for Result type using mapping Error
    template<typename T>
    using Result = common::Result<T, Error>;

} // namespace parser::mapping

#endif // NEBULA_MAPPER_MAPPING_ERRORS_HPP