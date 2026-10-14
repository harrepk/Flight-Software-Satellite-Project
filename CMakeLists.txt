cmake_minimum_required(VERSION 3.16)
project(cansat_xbee_comms VERSION 1.0.0 LANGUAGES CXX)

option(CANSAT_COMMS_BUILD_TESTS "Build the Google Test suite" ON)

# ---------------------------------------------------------------------------
# Library: no dependencies on hardware, UART drivers or the heap.
# ---------------------------------------------------------------------------
add_library(cansat_comms STATIC
    src/common.cpp
    src/xbee_frame.cpp
    src/xbee_frame_reader.cpp
    src/payload.cpp
)
add_library(cansat::comms ALIAS cansat_comms)

target_include_directories(cansat_comms PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_compile_features(cansat_comms PUBLIC cxx_std_17)

if(MSVC)
    target_compile_options(cansat_comms PRIVATE /W4 /permissive-)
else()
    target_compile_options(cansat_comms PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
endif()

# ---------------------------------------------------------------------------
# Tests (only when this is the top-level project)
# ---------------------------------------------------------------------------
if(CANSAT_COMMS_BUILD_TESTS AND CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    enable_testing()

    include(FetchContent)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    # Use the shared CRT on Windows so gtest matches the test executable.
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    add_executable(cansat_comms_tests
        tests/test_xbee_frame.cpp
        tests/test_frame_reader.cpp
        tests/test_payload.cpp
        tests/test_end_to_end.cpp
    )
    target_link_libraries(cansat_comms_tests PRIVATE cansat::comms GTest::gtest_main)

    include(GoogleTest)
    gtest_discover_tests(cansat_comms_tests)
endif()
