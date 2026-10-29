
cmake_minimum_required(VERSION 3.20)

project(Team14FileCompressionTool LANGUAGES CXX)

# Require C++17 without compiler-specific language extensions.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Apply compiler warnings.
function(team14_enable_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE
            /W4
            /permissive-
            /utf-8
        )
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
        target_compile_options(${target} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
        )
    endif()
endfunction()

# Core compression library.
add_library(team14_core STATIC
    src/archive.cpp
    src/controller.cpp
    src/file_io.cpp
    src/huffman.cpp
    src/validation.cpp
)

target_include_directories(team14_core
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_compile_features(team14_core PUBLIC cxx_std_17)

team14_enable_warnings(team14_core)

# Command-line executable.
add_executable(huf src/main.cpp)

target_link_libraries(huf PRIVATE team14_core)

team14_enable_warnings(huf)

include(CTest)

if(BUILD_TESTING)

    function(team14_add_module_test name source)
        set(target "team14_test_${name}")

        add_executable(${target} "${source}")

        target_link_libraries(${target} PRIVATE team14_core)

        team14_enable_warnings(${target})

        # Tests use TEST_CHECK, which remains active in Release builds.
        # No /UNDEBUG or -UNDEBUG option is needed.

        # Isolate temporary files for each test.
        set(work "${CMAKE_CURRENT_BINARY_DIR}/test-work/${name}")

        file(MAKE_DIRECTORY "${work}")

        add_test(
            NAME "team14.${name}"
            COMMAND ${target}
        )

        set_tests_properties("team14.${name}" PROPERTIES
            WORKING_DIRECTORY "${work}"
            ENVIRONMENT "TMPDIR=${work};TMP=${work};TEMP=${work}"
        )
    endfunction()

    # Module tests.
    team14_add_module_test(frequency tests/test_frequency.cpp)
    team14_add_module_test(file_io tests/test_file_io.cpp)
    team14_add_module_test(archive tests/test_archive.cpp)
    team14_add_module_test(validation tests/test_validation.cpp)
    team14_add_module_test(security_logging tests/test_security_logging.cpp)
    team14_add_module_test(round_trip tests/test_round_trip.cpp)
    team14_add_module_test(output_limits tests/test_output_limits.cpp)

    # CLI help test.
    add_test(
        NAME team14.cli_help
        COMMAND ${CMAKE_COMMAND}
            "-DHUF_EXECUTABLE=$<TARGET_FILE:huf>"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_cli.cmake"
    )

endif()
