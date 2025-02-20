#ifndef NEBULA_MAPPER_TRANSFORM_ENGINE_INL
#define NEBULA_MAPPER_TRANSFORM_ENGINE_INL

#include <string>
#include <stdexcept>
#include <type_traits>

namespace transformer {
    namespace detail {

        template<typename T>
        Result<T> convert_value(const TransformValue& value) {
            try {
                if (const auto* val = std::get_if<T>(&value.value)) {
                    return *val;
                }

                // Handle type conversion if needed
                if constexpr (std::is_same_v<T, std::string>) {
                    return std::visit([](const auto& v) -> std::string {
                        using V = std::decay_t<decltype(v)>;
                        if constexpr (std::is_same_v<V, std::string>) {
                            return v;
                        } else {
                            return std::to_string(v);
                        }
                    }, value.value);
                }
                else if constexpr (std::is_arithmetic_v<T>) {
                    return std::visit([](const auto& v) -> T {
                        using V = std::decay_t<decltype(v)>;
                        if constexpr (std::is_arithmetic_v<V>) {
                            return static_cast<T>(v);
                        }
                        else if constexpr (std::is_same_v<V, std::string>) {
                            return static_cast<T>(std::stod(v));
                        }
                        throw std::runtime_error("Invalid type conversion");
                    }, value.value);
                }

                return TransformError{
                    "Cannot convert value to requested type",
                    std::nullopt,
                    std::nullopt
                };
            }
            catch (const std::exception& e) {
                return TransformError{
                    "Conversion error: " + std::string(e.what()),
                    std::nullopt,
                    std::nullopt
                };
            }
        }

    } // namespace detail
} // namespace transformer

#endif // NEBULA_MAPPER_TRANSFORM_ENGINE_INL