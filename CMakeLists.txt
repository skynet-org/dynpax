cmake_minimum_required(VERSION 3.28)

project(
        dynpax
        VERSION 0.1.0
        LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated-literal-operator")

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake)

include(Dependencies)
include(GNUInstallDirs)
include(BundleUtils)

add_executable(dynpax
        src/bin/dynpax.cpp
        src/lib/App.cpp
        src/lib/Executable.cpp
        src/lib/FileManager.cpp
        src/lib/linux/LinuxLibraryResolver.cpp
)

target_include_directories(dynpax PRIVATE "src/include")

target_link_libraries(dynpax PRIVATE CLI11::CLI11 LIEF::LIEF
        fmt::fmt-header-only)
install(
        TARGETS dynpax
        RUNTIME COMPONENT dynpax DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY COMPONENT dynpax DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE COMPONENT dynpax DESTINATION ${CMAKE_INSTALL_LIBDIR})

fixup_target_bundle(dynpax)
