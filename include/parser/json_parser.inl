#ifndef NEBULA_MAPPER_JSON_PARSER_INL
#define NEBULA_MAPPER_JSON_PARSER_INL

namespace parser::json {

    template<typename T>
    Result<T> get_value(const JsonDocument& j, const std::string& path) {
        auto segments = detail::split_path(path);
        auto result = detail::navigate_path(j, segments);

        if (std::holds_alternative<Error>(result)) {
            return std::get<Error>(result);
        }

        const auto& value = std::get<JsonDocument>(result);
        try {
            return value.get<T>();
        } catch (const JsonDocument::exception& e) {
            return Error{"Type conversion failed: " + std::string(e.what())};
        }
    }

    template<typename T>
    T get_value_or(const JsonDocument& j, const std::string& path, const T& default_value) {
        auto result = get_value<T>(j, path);
        if (std::holds_alternative<Error>(result)) {
            return default_value;
        }
        return std::get<T>(result);
    }

} // namespace parser::json

#endif // NEBULA_MAPPER_JSON_PARSER_INL