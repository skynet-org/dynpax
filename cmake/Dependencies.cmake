set(LIEF_GIT_URL "https://github.com/lief-project/LIEF.git")
set(LIEF_VERSION 0.17.6)
set(CLI11_VERSION v2.6.2)
set(FMT_VERSION 12.1.0)

include(FetchContent)

FetchContent_Declare(
        LIEF
        GIT_REPOSITORY "${LIEF_GIT_URL}"
        GIT_TAG ${LIEF_VERSION})

FetchContent_Declare(
        CLI11
        QUIET
        GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
        GIT_TAG ${CLI11_VERSION})

FetchContent_Declare(
        fmt
        QUIET
        GIT_REPOSITORY https://github.com/fmtlib/fmt.git
        GIT_TAG ${FMT_VERSION})

set(LIEF_LOGGING_DEBUG OFF CACHE BOOL "Enable LIEF debug logging")
set(LIEF_EXAMPLES OFF CACHE BOOL "Build LIEF examples")
set(LIEF_TESTS OFF CACHE BOOL "Build LIEF tests")
set(CLI11_BUILD_TESTS OFF CACHE BOOL "Build CLI11 tests")
set(CLI11_BUILD_EXAMPLES OFF CACHE BOOL "Build CLI11 examples")
set(CLI11_BUILD_DOCS OFF CACHE BOOL "Build CLI11 documentation")
set(FMT_TEST OFF CACHE BOOL "Build fmt tests")
set(FMT_CUDA_TEST OFF CACHE BOOL "Build fmt CUDA tests")
set(FMT_DOC OFF CACHE BOOL "Build fmt documentation")

FetchContent_MakeAvailable(LIEF CLI11 fmt)
