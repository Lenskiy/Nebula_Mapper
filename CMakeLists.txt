cmake_minimum_required(VERSION 3.14)
project(nebula_mapper
        VERSION 1.0.0
        DESCRIPTION "JSON to Nebula Graph Mapper"
        LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Options
option(BUILD_TESTS "Build tests" ON)
option(BUILD_EXAMPLES "Build examples" OFF)

# Find dependencies
find_package(nlohmann_json 3.11.2 REQUIRED)
find_package(yaml-cpp REQUIRED)

# Compiler warnings
if(MSVC)
    add_compile_options(/W4 /WX)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

# Define library sources
set(NEBULA_MAPPER_SOURCES
        src/parser/json_parser.cpp
        src/parser/yaml_parser.cpp
        src/parser/mapping_parser.cpp
        src/parser/validation/mapping_validation.cpp
        src/transformer/transform_engine.cpp
        src/graph/schema_manager.cpp
        src/graph/statement_generator.cpp
)

# Define library headers
set(NEBULA_MAPPER_HEADERS
        include/common/result.hpp
        include/parser/json_parser.hpp
        include/parser/yaml_parser.hpp
        include/parser/mapping_parser.hpp
        include/parser/mapping_errors.hpp
        include/parser/validation/mapping_validation.hpp
        include/transformer/transform_engine.hpp
        include/transformer/transform_engine.inl
        include/graph/schema_manager.hpp
        include/graph/statement_generator.hpp
)

# Define library headers
#set(NEBULA_MAPPER_HEADERS
#        include/parser/validation/mapping_validation.hpp
#        include/parser/dynamic_fields_utils.hpp
#        include/common/string_utils.hpp
#        include/common/yaml_error_utils.hpp
#        include/parser/json_parser.hpp
#        include/parser/yaml_parser.hpp
#        include/parser/mapping_parser.hpp
#        include/transformer/transform_engine.hpp
#        include/transformer/transform_engine.inl
#        include/graph/schema_manager.hpp
#        include/graph/statement_generator.hpp
#        src/parser/json_parser.cpp
#        src/parser/yaml_parser.cpp
#        src/parser/mapping_parser.cpp
#        src/transformer/transform_engine.cpp
#        src/graph/schema_manager.cpp
#        src/graph/statement_generator.cpp
#        include/graph/schema_manager.hpp
#        include/graph/statement_generator.hpp
#        include/transformer/transform_engine.hpp
#        src/parser/validation/mapping_validation.cpp
#        include/parser/mapping_parser.hpp
#        include/parser/yaml_parser.hpp
#        include/parser/json_parser.hpp
#        tests/parser/json_parser_test.cpp
#        include/common/result.hpp
#)

# Create library target
add_library(nebula_mapper_lib ${NEBULA_MAPPER_SOURCES} ${NEBULA_MAPPER_HEADERS})
add_library(NebulaMapper::Lib ALIAS nebula_mapper_lib)

# Set include directories for the library
target_include_directories(nebula_mapper_lib
        PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Link dependencies
target_link_libraries(nebula_mapper_lib
        PUBLIC
        nlohmann_json::nlohmann_json
        yaml-cpp
)

# Create executable target
add_executable(nebula_mapper src/main.cpp)
target_link_libraries(nebula_mapper
        PRIVATE
        NebulaMapper::Lib
)

# Tests configuration
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Installation rules
include(GNUInstallDirs)
install(TARGETS nebula_mapper_lib
        EXPORT nebula_mapper-targets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY include/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        FILES_MATCHING PATTERN "*.hpp"
)

install(EXPORT nebula_mapper-targets
        FILE NebulaMapperTargets.cmake
        NAMESPACE NebulaMapper::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/nebula_mapper
)