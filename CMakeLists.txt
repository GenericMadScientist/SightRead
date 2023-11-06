cmake_minimum_required(VERSION 3.15.0)

cmake_policy(SET CMP0091 NEW)

project(
    sightread
    VERSION 0.1.0
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

if (DEFINED EMSCRIPTEN)
	add_executable(sightread
		src/sightread/chartparser.cpp
		src/sightread/midiparser.cpp
		src/sightread/song.cpp
		src/sightread/bindings.cpp
		src/sightread/songparts.cpp
		src/sightread/tempomap.cpp
		src/sightread/detail/chart.cpp
		src/sightread/detail/chartconverter.cpp
		src/sightread/detail/midi.cpp
		src/sightread/detail/midiconverter.cpp
		src/sightread/detail/parserutil.cpp)

	# set(CMAKE_EXECUTABLE_SUFFIX ".wasm")

	set_target_properties(sightread PROPERTIES COMPILE_FLAGS "-lembind -Os -s MAIN_MODULE ")
	set_target_properties(sightread PROPERTIES LINK_FLAGS    "-lembind -Os -s WASM=1 -s MAIN_MODULE --no-entry")
else()
	add_library(sightread
		src/sightread/chartparser.cpp
		src/sightread/midiparser.cpp
		src/sightread/song.cpp
		src/sightread/test.cpp
		src/sightread/songparts.cpp
		src/sightread/tempomap.cpp
		src/sightread/detail/chart.cpp
		src/sightread/detail/chartconverter.cpp
		src/sightread/detail/midi.cpp
		src/sightread/detail/midiconverter.cpp
		src/sightread/detail/parserutil.cpp)
endif()

target_include_directories(sightread PUBLIC include PRIVATE src)
set_cpp_standard(sightread)

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
        src/sightread/detail/parserutil.cpp)

    target_include_directories(sightread_tests PRIVATE include src tests/sightread)
    target_link_directories(sightread_tests PRIVATE ${Boost_LIBRARY_DIRS})
    target_link_libraries(sightread_tests PRIVATE Boost::unit_test_framework)
    add_test(NAME sightread_tests COMMAND sightread_tests)
    set_cpp_standard(sightread_tests)
    target_compile_options(
        sightread_tests
        PRIVATE $<$<CXX_COMPILER_ID:Clang,AppleClang,GNU>:
        -Wall
        -Wextra
        -Werror
        -Wno-c++98-compat
        -Wno-unknown-warning-option>
        $<$<CXX_COMPILER_ID:MSVC>:
        /W4
        /WX>)

    set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake;${CMAKE_MODULE_PATH}")
    include(cmake/Sanitisers.cmake)
    enable_sanitisers(sightread_tests)
endif()
