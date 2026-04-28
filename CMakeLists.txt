cmake_minimum_required(VERSION 3.28)

project(
        dynpax
        VERSION 0.1.0
        LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated-literal-operator")

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake)

include(Dependencies)
option(BUILD_TESTING "Build Dynpax tests" OFF)
include(CTest)

add_library(dependencies INTERFACE)

target_link_libraries(dependencies INTERFACE CLI11::CLI11 LIEF::LIEF
            fmt::fmt-header-only)

if (NOT DEPENDENCIES_ONLY)
    include(GNUInstallDirs)
    include(BundleUtils)

    add_library(dynpax_core STATIC
            src/lib/App.cpp
            src/lib/BundleBuilder.cpp
            src/lib/BundleLayout.cpp
            src/lib/Executable.cpp
            src/lib/FakeRoot.cpp
            src/lib/Resolver.cpp
            src/lib/ELFCache.cpp
    )
    target_include_directories(dynpax_core PUBLIC "src/include")
    target_link_libraries(dynpax_core PUBLIC dependencies)

    add_library(dynpax_bundle_verifier STATIC
            src/lib/BundleVerifier.cpp
    )
    target_include_directories(dynpax_bundle_verifier PUBLIC "src/include")
    target_link_libraries(dynpax_bundle_verifier PUBLIC dynpax_core)

    add_executable(dynpax
            src/bin/dynpax.cpp
    )

    target_include_directories(dynpax PRIVATE "src/include")
    target_link_libraries(dynpax PRIVATE dynpax_core dynpax_bundle_verifier)

    install(
            TARGETS dynpax
            RUNTIME COMPONENT dynpax DESTINATION ${CMAKE_INSTALL_BINDIR}
            LIBRARY COMPONENT dynpax DESTINATION ${CMAKE_INSTALL_LIBDIR}
            ARCHIVE COMPONENT dynpax DESTINATION ${CMAKE_INSTALL_LIBDIR})

    fixup_target_bundle(dynpax)

    if (BUILD_TESTING)
        set(DYNPAX_FIXTURE_ROOT "${CMAKE_CURRENT_BINARY_DIR}/tests/fixtures")
        set(DYNPAX_FIXTURE_BIN_DIR "${DYNPAX_FIXTURE_ROOT}/bin")
        set(DYNPAX_FIXTURE_LIB_DIR "${DYNPAX_FIXTURE_ROOT}/lib")
                option(DYNPAX_ENABLE_MUSL_FIXTURE "Build the musl fixture when musl-gcc is available" ON)
                option(DYNPAX_ENABLE_DOCKER_MATRIX "Enable the opt-in Docker validation matrix target" OFF)

                find_program(DYNPAX_MUSL_GCC musl-gcc)
                find_program(DYNPAX_DOCKER_EXECUTABLE docker)

        add_library(dynpax_fixture_message SHARED
                tests/fixtures/src/shared/message.c
        )
        target_include_directories(dynpax_fixture_message PRIVATE "tests/fixtures/src/shared")
        set_target_properties(dynpax_fixture_message PROPERTIES
                OUTPUT_NAME "dynpaxmessage"
                VERSION 1.0.0
                SOVERSION 1
                LIBRARY_OUTPUT_DIRECTORY "${DYNPAX_FIXTURE_LIB_DIR}")

        add_library(dynpax_fixture_greet SHARED
                tests/fixtures/src/shared/greet.c
        )
        target_include_directories(dynpax_fixture_greet PRIVATE "tests/fixtures/src/shared")
        target_link_libraries(dynpax_fixture_greet PRIVATE dynpax_fixture_message)
        set_target_properties(dynpax_fixture_greet PROPERTIES
                OUTPUT_NAME "dynpaxgreet"
                VERSION 1.2.0
                SOVERSION 1
                LIBRARY_OUTPUT_DIRECTORY "${DYNPAX_FIXTURE_LIB_DIR}")

        add_executable(dynpax_fixture_hello_glibc
                tests/fixtures/src/hello_glibc/main.c
        )
        set_target_properties(dynpax_fixture_hello_glibc PROPERTIES
                OUTPUT_NAME "hello_glibc"
                RUNTIME_OUTPUT_DIRECTORY "${DYNPAX_FIXTURE_BIN_DIR}")

        add_executable(dynpax_fixture_hello_transitive_glibc
                tests/fixtures/src/hello_transitive_glibc/main.c
        )
        target_include_directories(dynpax_fixture_hello_transitive_glibc PRIVATE "tests/fixtures/src/shared")
        target_link_libraries(dynpax_fixture_hello_transitive_glibc PRIVATE dynpax_fixture_greet)
        set_target_properties(dynpax_fixture_hello_transitive_glibc PROPERTIES
                OUTPUT_NAME "hello_transitive_glibc"
                BUILD_RPATH "${DYNPAX_FIXTURE_LIB_DIR}"
                RUNTIME_OUTPUT_DIRECTORY "${DYNPAX_FIXTURE_BIN_DIR}")

        add_executable(dynpax_fixture_hello_soname_glibc
                tests/fixtures/src/hello_soname_glibc/main.c
        )
        target_include_directories(dynpax_fixture_hello_soname_glibc PRIVATE "tests/fixtures/src/shared")
        target_link_libraries(dynpax_fixture_hello_soname_glibc PRIVATE dynpax_fixture_greet)
        set_target_properties(dynpax_fixture_hello_soname_glibc PROPERTIES
                OUTPUT_NAME "hello_soname_glibc"
                BUILD_RPATH "${DYNPAX_FIXTURE_LIB_DIR}"
                RUNTIME_OUTPUT_DIRECTORY "${DYNPAX_FIXTURE_BIN_DIR}")

        add_executable(dynpax_fixture_hello_abs_runpath_glibc
                tests/fixtures/src/hello_abs_runpath_glibc/main.c
        )
        target_include_directories(dynpax_fixture_hello_abs_runpath_glibc PRIVATE "tests/fixtures/src/shared")
        target_link_libraries(dynpax_fixture_hello_abs_runpath_glibc PRIVATE dynpax_fixture_greet)
        target_link_options(dynpax_fixture_hello_abs_runpath_glibc PRIVATE
                "-Wl,--enable-new-dtags,-rpath,${DYNPAX_FIXTURE_LIB_DIR}")
        set_target_properties(dynpax_fixture_hello_abs_runpath_glibc PROPERTIES
                OUTPUT_NAME "hello_abs_runpath_glibc"
                BUILD_RPATH "${DYNPAX_FIXTURE_LIB_DIR}"
                RUNTIME_OUTPUT_DIRECTORY "${DYNPAX_FIXTURE_BIN_DIR}")

        set(DYNPAX_FIXTURE_HELLO_MUSL "")
        if (DYNPAX_ENABLE_MUSL_FIXTURE AND DYNPAX_MUSL_GCC)
            set(DYNPAX_FIXTURE_HELLO_MUSL "${DYNPAX_FIXTURE_BIN_DIR}/hello_musl")
            add_custom_command(
                    OUTPUT "${DYNPAX_FIXTURE_HELLO_MUSL}"
                    COMMAND "${CMAKE_COMMAND}" -E make_directory "${DYNPAX_FIXTURE_BIN_DIR}"
                    COMMAND "${DYNPAX_MUSL_GCC}" -O2 -o "${DYNPAX_FIXTURE_HELLO_MUSL}"
                            "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/src/hello_musl/main.c"
                    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/src/hello_musl/main.c"
                    VERBATIM)
            add_custom_target(dynpax_fixture_hello_musl
                    DEPENDS "${DYNPAX_FIXTURE_HELLO_MUSL}")
        elseif (DYNPAX_ENABLE_MUSL_FIXTURE)
            message(STATUS "musl-gcc not found; skipping dynpax_fixture_hello_musl")
        endif ()

        add_library(dynpax_fixture_no_interp_so SHARED
                tests/fixtures/src/hello_no_interp_so/no_interp.c
        )
        set_target_properties(dynpax_fixture_no_interp_so PROPERTIES
                OUTPUT_NAME "dynpaxnointerp"
                VERSION 1.0.0
                SOVERSION 1
                LIBRARY_OUTPUT_DIRECTORY "${DYNPAX_FIXTURE_LIB_DIR}")

        add_executable(dynpax_resolver_tests
                tests/unit/ResolverTests.cpp
                src/lib/Resolver.cpp
        )

        target_include_directories(dynpax_resolver_tests PRIVATE "src/include")
        target_link_libraries(dynpax_resolver_tests PRIVATE fmt::fmt-header-only)

        add_test(NAME dynpax.resolver COMMAND dynpax_resolver_tests)

        add_executable(dynpax_bundle_verifier_tests
                tests/integration/BundleVerifierTests.cpp
        )
        target_include_directories(dynpax_bundle_verifier_tests PRIVATE "src/include")
        target_link_libraries(dynpax_bundle_verifier_tests PRIVATE dynpax_core dynpax_bundle_verifier)
        target_compile_definitions(dynpax_bundle_verifier_tests PRIVATE
                DYNPAX_FIXTURE_HELLO_SONAME="$<TARGET_FILE:dynpax_fixture_hello_soname_glibc>"
                DYNPAX_FIXTURE_HELLO_ABS_RUNPATH="$<TARGET_FILE:dynpax_fixture_hello_abs_runpath_glibc>"
                DYNPAX_FIXTURE_LIB_DIR="${DYNPAX_FIXTURE_LIB_DIR}")
        add_dependencies(dynpax_bundle_verifier_tests
                dynpax_fixture_message
                dynpax_fixture_greet
                dynpax_fixture_hello_glibc
                dynpax_fixture_hello_transitive_glibc
                dynpax_fixture_hello_soname_glibc
                dynpax_fixture_hello_abs_runpath_glibc
                dynpax_fixture_no_interp_so)

        add_test(NAME dynpax.bundle_verifier COMMAND dynpax_bundle_verifier_tests)

        if (DYNPAX_ENABLE_DOCKER_MATRIX)
            if (NOT DYNPAX_DOCKER_EXECUTABLE)
                message(FATAL_ERROR "DYNPAX_ENABLE_DOCKER_MATRIX=ON requires docker")
            endif ()

            set(DYNPAX_DOCKER_MATRIX_ARGS
                    --docker "${DYNPAX_DOCKER_EXECUTABLE}"
                    --dynpax "$<TARGET_FILE:dynpax>"
                    --glibc-fixture "$<TARGET_FILE:dynpax_fixture_hello_soname_glibc>")
            set(DYNPAX_DOCKER_MATRIX_DEPENDS
                    dynpax
                    dynpax_fixture_hello_soname_glibc)

            if (TARGET dynpax_fixture_hello_musl)
                list(APPEND DYNPAX_DOCKER_MATRIX_ARGS
                        --musl-fixture "${DYNPAX_FIXTURE_HELLO_MUSL}")
                list(APPEND DYNPAX_DOCKER_MATRIX_DEPENDS dynpax_fixture_hello_musl)
            endif ()

            add_custom_target(dynpax_docker_matrix
                    COMMAND "${CMAKE_COMMAND}" -E env
                            "DYNPAX_DOCKER_LOG_DIR=${CMAKE_CURRENT_BINARY_DIR}/tests/docker/logs"
                            "DYNPAX_DOCKER_WORK_ROOT=${CMAKE_CURRENT_BINARY_DIR}/tests/docker/work"
                            bash "${CMAKE_CURRENT_SOURCE_DIR}/tests/docker/run-matrix.sh"
                            ${DYNPAX_DOCKER_MATRIX_ARGS}
                    DEPENDS ${DYNPAX_DOCKER_MATRIX_DEPENDS}
                    USES_TERMINAL
                    VERBATIM)
        endif ()
    endif ()
endif ()
