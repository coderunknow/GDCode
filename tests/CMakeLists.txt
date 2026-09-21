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
