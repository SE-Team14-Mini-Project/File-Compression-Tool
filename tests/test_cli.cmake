if(NOT DEFINED HUF_EXECUTABLE)
    message(FATAL_ERROR "HUF_EXECUTABLE was not provided")
endif()

function(run_cli_test test_name expected_code)
    execute_process(
        COMMAND "${HUF_EXECUTABLE}" ${ARGN}
        RESULT_VARIABLE actual_code
        OUTPUT_VARIABLE stdout
        ERROR_VARIABLE stderr
    )

    if(NOT "${actual_code}" STREQUAL "${expected_code}")
        message(FATAL_ERROR
            "${test_name}: expected exit code ${expected_code}, got ${actual_code}\n"
            "stdout:\n${stdout}\nstderr:\n${stderr}"
        )
    endif()

    set(TEST_STDOUT "${stdout}" PARENT_SCOPE)
    set(TEST_STDERR "${stderr}" PARENT_SCOPE)
endfunction()

function(require_text test_name content expected_text)
    string(FIND "${content}" "${expected_text}" position)

    if(position EQUAL -1)
        message(FATAL_ERROR
            "${test_name}: expected text not found: '${expected_text}'\n"
            "Actual output:\n${content}"
        )
    endif()
endfunction()

# All supported help flags must display the available commands.
foreach(help_arg IN ITEMS help --help -h)
    run_cli_test("help_${help_arg}" 0 "${help_arg}")

    foreach(expected IN ITEMS
        "compress <input> <archive>"
        "decompress <archive> <output>"
        "info <archive>"
        "help"
    )
        require_text("help_${help_arg}" "${TEST_STDOUT}" "${expected}")
    endforeach()
endforeach()

# Missing command should explain the problem and show general usage.
run_cli_test("missing_command" 2)
require_text("missing_command" "${TEST_STDERR}" "Error: missing command.")
require_text("missing_command" "${TEST_STDERR}" "huf compress <input> <archive>")

# Each operation must provide actionable guidance when arguments are missing.
run_cli_test("compress_missing_args" 2 compress)
require_text("compress_missing_args" "${TEST_STDERR}"
    "compress requires an input file and an archive path.")
require_text("compress_missing_args" "${TEST_STDERR}"
    "Usage: huf compress <input> <archive>")

run_cli_test("decompress_missing_args" 2 decompress)
require_text("decompress_missing_args" "${TEST_STDERR}"
    "decompress requires an archive and an output path.")
require_text("decompress_missing_args" "${TEST_STDERR}"
    "Usage: huf decompress <archive> <output>")

run_cli_test("info_missing_args" 2 info)
require_text("info_missing_args" "${TEST_STDERR}"
    "info requires exactly one archive path.")
require_text("info_missing_args" "${TEST_STDERR}"
    "Usage: huf info <archive>")

# Unknown commands must be identified and general usage displayed.
run_cli_test("unknown_command" 2 unknown)
require_text("unknown_command" "${TEST_STDERR}"
    "Error: unknown command 'unknown'.")
require_text("unknown_command" "${TEST_STDERR}"
    "huf decompress <archive> <output>")

message(STATUS "All CLI help and usage tests passed.")
