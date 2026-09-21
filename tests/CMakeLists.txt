set(GDCODE_TESTS
    test_lexer
    test_parser
    test_semantics
    test_generation
    test_encoder
    test_diagnostics
    test_golden
)

foreach(name ${GDCODE_TESTS})
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE gdcode_core)
    target_compile_definitions(${name} PRIVATE
        GDCODE_TEST_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME ${name} COMMAND ${name})
endforeach()

# Compile the actual navigation handoff against a narrow GD API test double.
# This checks our contract, not native pause/resume or cocos input dispatch.
add_executable(test_navigation test_navigation.cpp ../src/backend/LevelNavigation.cpp)
target_include_directories(test_navigation PRIVATE stubs)
add_test(NAME test_navigation COMMAND test_navigation)

add_test(NAME test_cli COMMAND ${CMAKE_COMMAND}
    -DCLI=$<TARGET_FILE:gdcode-cli> -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}
    -P ${CMAKE_CURRENT_SOURCE_DIR}/test_cli.cmake)
