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

FetchContent_MakeAvailable(LIEF CLI11 fmt)
