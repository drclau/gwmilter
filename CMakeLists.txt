cmake_minimum_required(VERSION 3.13.4)
if(POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif()

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Setting build type to 'Release' as none was specified.")
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
    # Set the possible values of build type for cmake-gui
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

project(gwmilter VERSION 0.1 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_VERBOSE_MAKEFILE OFF)
set(THIRD_PARTY_DIR "${PROJECT_SOURCE_DIR}/third_party" CACHE PATH "Path to third-party sources")

option(WERROR "Treat warnings as errors" ON)

include(ExternalProject)

# Find necessary packages
find_package(PkgConfig REQUIRED)
find_package(CURL REQUIRED)
find_package(fmt REQUIRED)
find_package(spdlog REQUIRED)

# Use pkg-config to find and configure GLib and GMime
pkg_check_modules(GLIB REQUIRED IMPORTED_TARGET glib-2.0)
pkg_check_modules(GMIME REQUIRED IMPORTED_TARGET gmime-3.0)

# Find paths and libraries for libmilter
find_path(MILTER_INCLUDE_DIR NAMES libmilter/mfapi.h)
find_library(MILTER_LIBRARY NAMES milter)

# Helper function to add autotools-based external projects
function(add_autotools_external_project NAME LIBNAME GIT_REPO GIT_TAG CPPFLAGS_VAR LDFLAGS_VAR)
    string(TOLOWER ${NAME} name_lower)
    set(SOURCE_DIR "${THIRD_PARTY_DIR}/${LIBNAME}")
    set(INSTALL_DIR "${CMAKE_BINARY_DIR}/_deps/${name_lower}")

    set(CONFIGURE_FLAGS "--prefix=${INSTALL_DIR}")
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(CONFIGURE_FLAGS "${CONFIGURE_FLAGS} --enable-debug")
    endif()

    if(EXISTS "${SOURCE_DIR}/configure.ac")
        set(DOWNLOAD_ARGS DOWNLOAD_COMMAND "")
    else()
        set(DOWNLOAD_ARGS GIT_REPOSITORY ${GIT_REPO} GIT_TAG ${GIT_TAG})
    endif()

    ExternalProject_Add(${name_lower}_external
        SOURCE_DIR "${SOURCE_DIR}"
        ${DOWNLOAD_ARGS}
        UPDATE_COMMAND ""
        CONFIGURE_COMMAND
            ${CMAKE_COMMAND} -E env
            "CPPFLAGS=${${CPPFLAGS_VAR}}"
            "LDFLAGS=${${LDFLAGS_VAR}}"
            sh -c "test -x ./configure || ./bootstrap && ./configure ${CONFIGURE_FLAGS}"
        BUILD_COMMAND ${CMAKE_MAKE_PROGRAM}
        INSTALL_COMMAND ${CMAKE_MAKE_PROGRAM} install
        BUILD_IN_SOURCE 1
    )

    set(${NAME}_INCLUDE_DIR "${INSTALL_DIR}/include" PARENT_SCOPE)
    set(${NAME}_LIBRARY "${INSTALL_DIR}/lib/${LIBNAME}${CMAKE_SHARED_LIBRARY_SUFFIX}" PARENT_SCOPE)
    set(${NAME}_INSTALL_DIR "${INSTALL_DIR}" PARENT_SCOPE)
endfunction()

# Find paths and libraries for EGPGCRYPT and EPDFCRYPT
option(FETCH_EXTERNAL_LIBS "Allow git cloning external libraries if not found" OFF)
set(EGPGCRYPT_PATH "" CACHE PATH "Path to libegpgcrypt installation prefix (contains include/ and lib/)")
set(EPDFCRYPT_PATH "" CACHE PATH "Path to libepdfcrypt installation prefix (contains include/ and lib/)")
set(EGPGCRYPT_GIT_REPOSITORY "https://github.com/drclau/libegpgcrypt" CACHE STRING "libegpgcrypt git repository")
set(EPDFCRYPT_GIT_REPOSITORY "https://github.com/drclau/libepdfcrypt" CACHE STRING "libepdfcrypt git repository")
set(EGPGCRYPT_GIT_TAG "HEAD" CACHE STRING "libegpgcrypt git tag/commit")
set(EPDFCRYPT_GIT_TAG "HEAD" CACHE STRING "libepdfcrypt git tag/commit")
set(EGPGCRYPT_CPPFLAGS "" CACHE STRING "Extra CPPFLAGS for libegpgcrypt configure (e.g. -I/usr/local/include)")
set(EGPGCRYPT_LDFLAGS "" CACHE STRING "Extra LDFLAGS for libegpgcrypt configure (e.g. -L/usr/local/lib)")
set(EPDFCRYPT_CPPFLAGS "" CACHE STRING "Extra CPPFLAGS for libepdfcrypt configure (e.g. -I/usr/local/include)")
set(EPDFCRYPT_LDFLAGS "" CACHE STRING "Extra LDFLAGS for libepdfcrypt configure (e.g. -L/usr/local/lib)")

set(USE_EGPGCRYPT_EXTERNAL OFF)
set(USE_EPDFCRYPT_EXTERNAL OFF)

if(EGPGCRYPT_PATH)
    find_path(EGPGCRYPT_INCLUDE_DIR NAMES crypto.hpp HINTS ${EGPGCRYPT_PATH}/include)
    find_library(EGPGCRYPT_LIBRARY NAMES egpgcrypt HINTS ${EGPGCRYPT_PATH}/lib)
    if(NOT EGPGCRYPT_INCLUDE_DIR OR NOT EGPGCRYPT_LIBRARY)
        message(FATAL_ERROR "EGPGCRYPT_PATH is set to '${EGPGCRYPT_PATH}', but libegpgcrypt was not found in include/ or lib/.")
    endif()
    message(STATUS "Using pre-installed libegpgcrypt from ${EGPGCRYPT_PATH}")
else()
    set(USE_EGPGCRYPT_EXTERNAL ON)
    file(MAKE_DIRECTORY "${THIRD_PARTY_DIR}")
    set(EGPGCRYPT_SOURCE_DIR "${THIRD_PARTY_DIR}/libegpgcrypt")
    if(EXISTS "${EGPGCRYPT_SOURCE_DIR}/configure.ac")
        message(STATUS "Building libegpgcrypt from vendored sources in ${EGPGCRYPT_SOURCE_DIR}")
    elseif(FETCH_EXTERNAL_LIBS)
        message(STATUS "Building libegpgcrypt from git repository ${EGPGCRYPT_GIT_REPOSITORY} (${EGPGCRYPT_GIT_TAG})")
    else()
        message(FATAL_ERROR "libegpgcrypt not found. Options:\n"
            "  1. Install libegpgcrypt and set -DEGPGCRYPT_PATH=/path/to/installation\n"
            "  2. Place sources in ${EGPGCRYPT_SOURCE_DIR}\n"
            "  3. Allow git clone with -DFETCH_EXTERNAL_LIBS=ON")
    endif()
    add_autotools_external_project(EGPGCRYPT libegpgcrypt
        ${EGPGCRYPT_GIT_REPOSITORY} ${EGPGCRYPT_GIT_TAG}
        EGPGCRYPT_CPPFLAGS EGPGCRYPT_LDFLAGS)
endif()

if(EPDFCRYPT_PATH)
    find_path(EPDFCRYPT_INCLUDE_DIR NAMES epdf.hpp HINTS ${EPDFCRYPT_PATH}/include)
    find_library(EPDFCRYPT_LIBRARY NAMES epdfcrypt HINTS ${EPDFCRYPT_PATH}/lib)
    if(NOT EPDFCRYPT_INCLUDE_DIR OR NOT EPDFCRYPT_LIBRARY)
        message(FATAL_ERROR "EPDFCRYPT_PATH is set to '${EPDFCRYPT_PATH}', but libepdfcrypt was not found in include/ or lib/.")
    endif()
    message(STATUS "Using pre-installed libepdfcrypt from ${EPDFCRYPT_PATH}")
else()
    set(USE_EPDFCRYPT_EXTERNAL ON)
    file(MAKE_DIRECTORY "${THIRD_PARTY_DIR}")

    # Help configure find libharu on macOS Homebrew if flags not provided
    if(APPLE AND EPDFCRYPT_CPPFLAGS STREQUAL "" AND EPDFCRYPT_LDFLAGS STREQUAL "")
        find_path(HPDF_INCLUDE_DIR NAMES hpdf.h)
        find_library(HPDF_LIBRARY NAMES hpdf)
        if(HPDF_INCLUDE_DIR AND HPDF_LIBRARY)
            set(EPDFCRYPT_CPPFLAGS "-I${HPDF_INCLUDE_DIR}")
            get_filename_component(HPDF_LIBRARY_DIR "${HPDF_LIBRARY}" DIRECTORY)
            set(EPDFCRYPT_LDFLAGS "-L${HPDF_LIBRARY_DIR}")
            message(STATUS "Found libharu at ${HPDF_LIBRARY_DIR} (passing to libepdfcrypt configure)")
        endif()
    endif()

    set(EPDFCRYPT_SOURCE_DIR "${THIRD_PARTY_DIR}/libepdfcrypt")
    if(EXISTS "${EPDFCRYPT_SOURCE_DIR}/configure.ac")
        message(STATUS "Building libepdfcrypt from vendored sources in ${EPDFCRYPT_SOURCE_DIR}")
    elseif(FETCH_EXTERNAL_LIBS)
        message(STATUS "Building libepdfcrypt from git repository ${EPDFCRYPT_GIT_REPOSITORY} (${EPDFCRYPT_GIT_TAG})")
    else()
        message(FATAL_ERROR "libepdfcrypt not found. Options:\n"
            "  1. Install libepdfcrypt and set -DEPDFCRYPT_PATH=/path/to/installation\n"
            "  2. Place sources in ${EPDFCRYPT_SOURCE_DIR}\n"
            "  3. Allow git clone with -DFETCH_EXTERNAL_LIBS=ON")
    endif()

    add_autotools_external_project(EPDFCRYPT libepdfcrypt
        ${EPDFCRYPT_GIT_REPOSITORY} ${EPDFCRYPT_GIT_TAG}
        EPDFCRYPT_CPPFLAGS EPDFCRYPT_LDFLAGS)
endif()

# Define the executable and its source files
set(GWMILTER_SOURCES
    src/main.cpp
    src/signal_manager.hpp
    src/signal_manager.cpp
    src/cfg2/section_registry.hpp
    src/cfg2/section_registry.cpp
    src/cfg2/config.hpp
    src/cfg2/config.cpp
    src/cfg2/config_manager.hpp
    src/cfg2/config_manager.cpp
    src/cfg2/ini_reader.hpp
    src/cfg2/ini_reader.cpp
    src/handlers/body_handler.hpp
    src/handlers/body_handler.cpp
    src/handlers/headers.hpp
    src/handlers/noop_body_handler.cpp
    src/handlers/pdf_body_handler.cpp
    src/handlers/pgp_body_handler.cpp
    src/handlers/smime_body_handler.cpp
    src/milter/milter.hpp
    src/milter/milter.cpp
    src/milter/milter_callbacks.hpp
    src/milter/milter_callbacks.cpp
    src/milter/milter_connection.hpp
    src/milter/milter_connection.cpp
    src/milter/milter_exception.hpp
    src/milter/milter_message.hpp
    src/milter/milter_message.cpp
    src/smtp/smtp_client.hpp
    src/smtp/smtp_client.cpp
    src/utils/string.hpp
    src/utils/string.cpp
    src/utils/uid_generator.cpp
    src/utils/uid_generator.hpp
    src/utils/dump_email.hpp
    src/utils/dump_email.cpp
    src/logger/logger.hpp
    src/logger/spdlog_init.hpp
    src/logger/spdlog_init.cpp
)

add_executable(gwmilter ${GWMILTER_SOURCES})

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(gwmilter PRIVATE -Wall)
    if(WERROR)
        target_compile_options(gwmilter PRIVATE -Werror)
    endif()
endif()

# SimpleIni is required for cfg2 INI parsing.
# Prefer a local copy; only fetch when explicitly enabled.
option(FETCH_SIMPLEINI "Fetch SimpleIni if not available locally" OFF)
set(SIMPLEINI_GIT_TAG "v4.25" CACHE STRING "SimpleIni git tag to fetch")
set(simpleini_local_dir "${THIRD_PARTY_DIR}/simpleini")
if(EXISTS "${simpleini_local_dir}/SimpleIni.h")
    set(simpleini_SOURCE_DIR "${simpleini_local_dir}")
    message(STATUS "Using vendored SimpleIni from ${simpleini_local_dir}")
elseif(FETCH_SIMPLEINI)
    message(STATUS "Fetching SimpleIni from https://github.com/brofield/simpleini.git (${SIMPLEINI_GIT_TAG})")
    include(FetchContent)
    FetchContent_Declare(
        simpleini
        GIT_REPOSITORY https://github.com/brofield/simpleini.git
        GIT_TAG ${SIMPLEINI_GIT_TAG}
    )
    FetchContent_MakeAvailable(simpleini)
else()
    message(FATAL_ERROR "SimpleIni not found. Provide ${THIRD_PARTY_DIR}/simpleini or set FETCH_SIMPLEINI=ON to fetch tag ${SIMPLEINI_GIT_TAG}.")
endif()

# Toggle cfg2 module and utilities (demo/tests) on demand; default OFF while migrating
option(ENABLE_CFG2 "Build cfg2 module and utilities (demo/tests)" OFF)

# Set include directories for gwmilter target
target_include_directories(gwmilter PRIVATE
    ${PROJECT_SOURCE_DIR}/src
    ${CURL_INCLUDE_DIRS}
    ${MILTER_INCLUDE_DIR}
    ${GLIB_INCLUDE_DIRS}
    ${GMIME_INCLUDE_DIRS}
    ${EGPGCRYPT_INCLUDE_DIR}
    ${EPDFCRYPT_INCLUDE_DIR}
    ${FMT_INCLUDE_DIRS}
    ${simpleini_SOURCE_DIR}
)

# Link libraries to gwmilter target
target_link_libraries(gwmilter PRIVATE
    CURL::libcurl
    fmt::fmt
    ${MILTER_LIBRARY}
    PkgConfig::GLIB
    PkgConfig::GMIME
    ${EGPGCRYPT_LIBRARY}
    ${EPDFCRYPT_LIBRARY}
    spdlog::spdlog
)

set(gwmilter_build_rpath "")
if(USE_EGPGCRYPT_EXTERNAL)
    add_dependencies(gwmilter egpgcrypt_external)
    list(APPEND gwmilter_build_rpath "${EGPGCRYPT_INSTALL_DIR}/lib")
endif()
if(USE_EPDFCRYPT_EXTERNAL)
    add_dependencies(gwmilter epdfcrypt_external)
    list(APPEND gwmilter_build_rpath "${EPDFCRYPT_INSTALL_DIR}/lib")
endif()
if(gwmilter_build_rpath)
    set_target_properties(gwmilter PROPERTIES BUILD_RPATH "${gwmilter_build_rpath}")
endif()

# std::filesystem requires linking against a separate library on older compilers
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.1)
    target_link_libraries(gwmilter PRIVATE stdc++fs)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.0)
    target_link_libraries(gwmilter PRIVATE c++fs)
endif()

# Default test toggle: OFF unless explicitly enabled by user or CI
option(BUILD_TESTING "Enable building tests via CTest/GTest" OFF)

include(CTest)
if(BUILD_TESTING)
    enable_testing()
endif()

if(ENABLE_CFG2)
    find_package(Threads REQUIRED)

    # Add cfg2 demo executable with new modular structure
    add_executable(cfg2_demo
        src/cfg2/demo.cpp
        src/cfg2/section_registry.cpp
        src/cfg2/config.cpp
        src/cfg2/ini_reader.cpp
        src/cfg2/config_manager.cpp
        src/utils/string.cpp
    )

    target_include_directories(cfg2_demo PRIVATE
        ${PROJECT_SOURCE_DIR}/src
    ${simpleini_SOURCE_DIR}
)

    target_link_libraries(cfg2_demo
        PRIVATE
        spdlog::spdlog
        Threads::Threads
        fmt::fmt
    )

    set_target_properties(cfg2_demo PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
    )

    if(BUILD_TESTING)
        find_package(GTest REQUIRED)
        include(GoogleTest)

        # Add cfg2 tests executable with Google Test
        add_executable(cfg2_tests
            src/cfg2/deserializer_tests.cpp
            src/cfg2/dynamic_section_tests.cpp
            src/cfg2/config_tests.cpp
            src/cfg2/config_node_tests.cpp
            src/cfg2/ini_reader_tests.cpp
            src/cfg2/multiple_same_type_test.cpp
            src/cfg2/section_registry.cpp
            src/cfg2/config.cpp
            src/cfg2/ini_reader.cpp
            src/cfg2/config_manager.cpp
            src/utils/string.cpp
        )

        target_include_directories(cfg2_tests PRIVATE
            ${PROJECT_SOURCE_DIR}/src
            ${simpleini_SOURCE_DIR}
        )

        target_link_libraries(cfg2_tests PRIVATE
            GTest::gtest_main
            spdlog::spdlog
        )

        set_target_properties(cfg2_tests PROPERTIES
            CXX_STANDARD 17
            CXX_STANDARD_REQUIRED ON
        )

        # Register GoogleTest-based tests individually with CTest
        gtest_discover_tests(cfg2_tests
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            DISCOVERY_TIMEOUT 60
        )
    endif()
endif()
