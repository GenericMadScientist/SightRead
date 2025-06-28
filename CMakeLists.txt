cmake_minimum_required(VERSION 3.30.0)

cmake_policy(SET CMP0091 NEW)
cmake_policy(SET CMP0167 NEW)

project(
    sightread
    VERSION 0.5.1
    DESCRIPTION "A library to read .mid and .chart files for GH/RB-style games"
    LANGUAGES CXX)

# Require standard C++20
function(set_cpp_standard target)
    set_target_properties(
        ${target}
        PROPERTIES CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF)
endfunction()

function(add_warnings target)
    if("x${CMAKE_CXX_COMPILER_ID}" STREQUAL "xMSVC")
        target_compile_options(
            ${target} PRIVATE
            /W4
            /WX)
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL Clang AND "x${CMAKE_CXX_SIMULATE_ID}" STREQUAL "xMSVC")
        target_compile_options(
            ${target} PRIVATE
            /W4
            /WX
            -Wimplicit-int-float-conversion
            -Wmissing-noreturn
            -Wnewline-eof
            -Wsign-conversion
            -Wswitch-enum)
    else()
        target_compile_options(
            ${target} PRIVATE
            -Wall
            -Wextra
            -Werror
            -Wno-c++98-compat
            -Wno-unknown-warning-option)
    endif()
endfunction()

add_library(sightread
    src/sightread/chartparser.cpp
    src/sightread/midiparser.cpp
    src/sightread/song.cpp
    src/sightread/songparts.cpp
    src/sightread/tempomap.cpp
    src/sightread/detail/chart.cpp
    src/sightread/detail/chartconverter.cpp
    src/sightread/detail/midi.cpp
    src/sightread/detail/midiconverter.cpp
    src/sightread/detail/parserutil.cpp
    src/sightread/detail/qbmidi.cpp
    src/sightread/detail/qbmidiconverter.cpp)

target_include_directories(sightread PUBLIC include PRIVATE src)
set_cpp_standard(sightread)

option(SIGHTREAD_ENABLE_WARNINGS "Build SightRead with warnings" OFF)

if(SIGHTREAD_ENABLE_WARNINGS)
    add_warnings(sightread)
endif()

option(SIGHTREAD_BUILD_TESTS "Build SightRead tests" OFF)

if(SIGHTREAD_BUILD_TESTS)
    find_package(Boost 1.77 REQUIRED COMPONENTS unit_test_framework)

    enable_testing()
    add_executable(
        sightread_tests
        tests/sightread/test_main.cpp
        tests/sightread/chartparser_unittest.cpp
        tests/sightread/song_unittest.cpp
        tests/sightread/songparts_unittest.cpp
        tests/sightread/tempomap_unittest.cpp
        tests/sightread/time_unittest.cpp
        tests/sightread/detail/chart_unittest.cpp
        tests/sightread/detail/midi_unittest.cpp
        tests/sightread/detail/midiconverter_unittest.cpp
        src/sightread/chartparser.cpp
        src/sightread/song.cpp
        src/sightread/songparts.cpp
        src/sightread/tempomap.cpp
        src/sightread/detail/chart.cpp
        src/sightread/detail/chartconverter.cpp
        src/sightread/detail/midi.cpp
        src/sightread/detail/midiconverter.cpp
        src/sightread/detail/parserutil.cpp
        src/sightread/detail/qbmidi.cpp
        src/sightread/detail/qbmidiconverter.cpp)

    target_include_directories(sightread_tests PRIVATE include src tests/sightread)
    target_link_directories(sightread_tests PRIVATE ${Boost_LIBRARY_DIRS})
    target_link_libraries(sightread_tests PRIVATE Boost::unit_test_framework)
    add_test(NAME sightread_tests COMMAND sightread_tests)
    set_cpp_standard(sightread_tests)
    add_warnings(sightread_tests)

    set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake;${CMAKE_MODULE_PATH}")
    include(cmake/Sanitisers.cmake)
    enable_sanitisers(sightread_tests)
endif()
