cmake_minimum_required(VERSION 3.28)

project(
        caches
        VERSION 0.1.0
        LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated-literal-operator")

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR})

include(Dependencies)

add_library(dependencies INTERFACE)

target_link_libraries(dependencies INTERFACE CLI11::CLI11 LIEF::LIEF
            fmt::fmt-header-only)

if (NOT DEPENDENCIES_ONLY)
    message(FATAL_ERROR "This CMakeLists.txt is not meant to be built directly")
endif()
