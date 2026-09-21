# End-to-end CLI check: output formatting, exit codes and quiet compatibility.
file(WRITE "${WORK_DIR}/cli-error.gdx" "block 0 0\n\tspirke 4 1\r\n")
execute_process(COMMAND "${CLI}" "${WORK_DIR}/cli-error.gdx"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT status EQUAL 1 OR NOT output MATCHES "unknown-object" OR
   NOT output MATCHES "2 \\|     spirke 4 1" OR NOT output MATCHES "\\^" OR
   NOT output MATCHES "did you mean: 'spike'" OR NOT error STREQUAL "")
    message(FATAL_ERROR "bad diagnostic/exit: ${status}\n${output}\n${error}")
endif()
execute_process(COMMAND "${CLI}" "${WORK_DIR}/cli-error.gdx" --quiet
    RESULT_VARIABLE status OUTPUT_VARIABLE output)
if(NOT status EQUAL 1 OR NOT output STREQUAL "")
    message(FATAL_ERROR "quiet errors changed: ${status}: ${output}")
endif()
file(WRITE "${WORK_DIR}/cli-valid.gdx" "block 0 0\n")
execute_process(COMMAND "${CLI}" "${WORK_DIR}/cli-valid.gdx" --quiet --level-string
    RESULT_VARIABLE status OUTPUT_VARIABLE output)
if(NOT status EQUAL 0 OR NOT output MATCHES "1,1,2,15,3,15" OR output MATCHES "\\^")
    message(FATAL_ERROR "valid generation changed: ${status}: ${output}")
endif()
execute_process(COMMAND "${CLI}" "${WORK_DIR}/cli-valid.gdx"
    RESULT_VARIABLE status OUTPUT_VARIABLE output)
if(NOT status EQUAL 0 OR NOT output STREQUAL "OK: 1 object(s), 0 error(s)\n")
    message(FATAL_ERROR "valid summary changed: ${status}: ${output}")
endif()
