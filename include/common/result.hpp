#ifndef NEBULA_MAPPER_RESULT_HPP
#define NEBULA_MAPPER_RESULT_HPP

#include <string>
#include <variant>
#include <optional>

namespace common {

    // Base error type that can be extended
    struct Error {
        std::string message;
        std::optional<std::string> context{std::nullopt};

        Error(const std::string& msg,
              const std::optional<std::string>& ctx = std::nullopt)
            : message(msg), context(ctx) {}
    };

    // Generic result type for operations that can fail
    template<typename T, typename E = Error>
    using Result = std::variant<T, E>;

} // namespace common

#endif // NEBULA_MAPPER_RESULT_HPP