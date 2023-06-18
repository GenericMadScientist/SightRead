cmake_minimum_required(VERSION 3.15.0)

cmake_policy(SET CMP0091 NEW)

project(
    sightread
    VERSION 0.1.0
    DESCRIPTION "A library to read .mid and .chart files for GH/RB-style games"
    LANGUAGES CXX)

add_library(sightread)

target_include_directories(sightread PUBLIC include)

set_target_properties(
    sightread
    PROPERTIES CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF)
