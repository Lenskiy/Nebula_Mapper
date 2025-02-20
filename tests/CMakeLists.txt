find_package(GTest REQUIRED)
include(GoogleTest)

# Create test executables
add_executable(json_parser_test
        parser/json_parser_test.cpp
)

target_link_libraries(json_parser_test
        PRIVATE
        NebulaMapper::Lib
        GTest::gtest
        GTest::gtest_main
)

# Enable test discovery
gtest_discover_tests(json_parser_test
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        PROPERTIES
        ENVIRONMENT "TEST_DATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/test_data"
)

# Copy test data
file(COPY test_data/ DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/test_data)

# Add more test executables as they are implemented
set(TEST_TARGETS
        yaml_parser_test
        mapping_parser_test
        transform_engine_test
        schema_manager_test
        statement_generator_test
)

foreach(target ${TEST_TARGETS})
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${target}.cpp)
        add_executable(${target} ${target}.cpp)
        target_link_libraries(${target}
                PRIVATE
                NebulaMapper::Lib
                GTest::gtest
                GTest::gtest_main
        )
        gtest_discover_tests(${target}
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        )
    endif()
endforeach()