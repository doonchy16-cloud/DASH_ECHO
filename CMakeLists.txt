cmake_minimum_required(VERSION 3.21)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(ECHO_DASH_BUILD_CORE_TESTS "Build dependency-free ECHO_DASH core tests only" OFF)

if (ECHO_DASH_BUILD_CORE_TESTS)
    project(EchoDashCoreTests LANGUAGES CXX)
    enable_testing()
    add_executable(EchoDashCoreTests
        tests/cpp/test_main.cpp
        tests/cpp/test_current_invariants.cpp
        tests/cpp/test_core_types.cpp
        tests/cpp/test_time_policy.cpp
        src/EchoTimePolicy.cpp
    )
    target_include_directories(EchoDashCoreTests PRIVATE src tests/cpp)
    target_compile_features(EchoDashCoreTests PRIVATE cxx_std_23)
    add_test(NAME EchoDashCoreTests COMMAND EchoDashCoreTests)
    return()
endif()

if ("${CMAKE_SYSTEM_NAME}" STREQUAL "iOS" OR IOS)
    set(CMAKE_OSX_ARCHITECTURES "arm64")
else()
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64")
endif()

set(CMAKE_CXX_VISIBILITY_PRESET hidden)

project(EchoDash VERSION 1.1.3)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS src/*.cpp)
add_library(${PROJECT_NAME} SHARED ${SOURCES})

if (NOT DEFINED ENV{GEODE_SDK})
    message(FATAL_ERROR "Unable to find Geode SDK. Define GEODE_SDK to point to the installed Geode SDK.")
else()
    message(STATUS "Found Geode SDK: $ENV{GEODE_SDK}")
endif()

add_subdirectory($ENV{GEODE_SDK} ${CMAKE_CURRENT_BINARY_DIR}/geode)
setup_geode_mod(${PROJECT_NAME})