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
        list(APPEND CONFIGURE_FLAGS "--enable-debug")
    endif()
    list(JOIN CONFIGURE_FLAGS " " CONFIGURE_FLAGS_STR)
    set(CONFIGURE_SCRIPT "(test -x ./configure || ./bootstrap) && exec ./configure")
    string(APPEND CONFIGURE_SCRIPT " " "${CONFIGURE_FLAGS_STR}")

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
            sh -c "${CONFIGURE_SCRIPT}"
        BUILD_COMMAND ${CMAKE_MAKE_PROGRAM}
        INSTALL_COMMAND ${CMAKE_MAKE_PROGRAM} install
        BUILD_IN_SOURCE 1
    )

    set(${NAME}_INCLUDE_DIR "${INSTALL_DIR}/include" PARENT_SCOPE)
    set(${NAME}_LIBRARY "${INSTALL_DIR}/lib/${LIBNAME}${CMAKE_SHARED_LIBRARY_SUFFIX}" PARENT_SCOPE)
    set(${NAME}_INSTALL_DIR "${INSTALL_DIR}" PARENT_SCOPE)
endfunction()

# Macro to set up an autotools library with pre-installed or build-from-source options
# Sets: ${NAME}_INCLUDE_DIR, ${NAME}_LIBRARY, USE_${NAME}_EXTERNAL
macro(setup_autotools_library NAME LIBNAME HEADER_NAME DEFAULT_GIT_REPO DEFAULT_GIT_TAG)
    string(REGEX REPLACE "^lib" "" _lib_short_name "${LIBNAME}")
    set(${NAME}_PATH "" CACHE PATH "Path to ${LIBNAME} installation prefix (contains include/ and lib/)")
    set(${NAME}_GIT_REPOSITORY "${DEFAULT_GIT_REPO}" CACHE STRING "${LIBNAME} git repository")
    set(${NAME}_GIT_TAG "${DEFAULT_GIT_TAG}" CACHE STRING "${LIBNAME} git tag/commit")
    set(${NAME}_CPPFLAGS "" CACHE STRING "Extra CPPFLAGS for ${LIBNAME} configure")
    set(${NAME}_LDFLAGS "" CACHE STRING "Extra LDFLAGS for ${LIBNAME} configure")

    set(USE_${NAME}_EXTERNAL OFF)

    if(${NAME}_PATH)
        find_path(${NAME}_INCLUDE_DIR NAMES ${HEADER_NAME} HINTS ${${NAME}_PATH}/include)
        find_library(${NAME}_LIBRARY NAMES ${_lib_short_name} HINTS ${${NAME}_PATH}/lib)
        if(NOT ${NAME}_INCLUDE_DIR OR NOT ${NAME}_LIBRARY)
            message(FATAL_ERROR "${NAME}_PATH is set to '${${NAME}_PATH}', but ${LIBNAME} was not found in include/ or lib/.")
        endif()
        message(STATUS "Using pre-installed ${LIBNAME} from ${${NAME}_PATH}")
    else()
        set(USE_${NAME}_EXTERNAL ON)
        file(MAKE_DIRECTORY "${THIRD_PARTY_DIR}")
        set(${NAME}_SOURCE_DIR "${THIRD_PARTY_DIR}/${LIBNAME}")
        if(EXISTS "${${NAME}_SOURCE_DIR}/configure.ac")
            message(STATUS "Building ${LIBNAME} from vendored sources in ${${NAME}_SOURCE_DIR}")
        elseif(FETCH_EXTERNAL_LIBS)
            message(STATUS "Building ${LIBNAME} from git repository ${${NAME}_GIT_REPOSITORY} (${${NAME}_GIT_TAG})")
        else()
            message(FATAL_ERROR "${LIBNAME} not found. Options:\n"
                "  1. Install ${LIBNAME} and set -D${NAME}_PATH=/path/to/installation\n"
                "  2. Place sources in ${${NAME}_SOURCE_DIR}\n"
                "  3. Allow git clone with -DFETCH_EXTERNAL_LIBS=ON")
        endif()
        add_autotools_external_project(${NAME} ${LIBNAME}
            ${${NAME}_GIT_REPOSITORY} ${${NAME}_GIT_TAG}
            ${NAME}_CPPFLAGS ${NAME}_LDFLAGS)
    endif()
endmacro()

# External libraries: libegpgcrypt and libepdfcrypt
option(FETCH_EXTERNAL_LIBS "Allow git cloning external libraries if not found" OFF)

setup_autotools_library(EGPGCRYPT libegpgcrypt crypto.hpp
    "https://github.com/drclau/libegpgcrypt" "HEAD")

# Help configure find libharu on macOS Homebrew if flags not provided before setup_autotools_library
if(APPLE)
    if((NOT DEFINED EPDFCRYPT_PATH OR EPDFCRYPT_PATH STREQUAL "") AND
       (NOT DEFINED EPDFCRYPT_CPPFLAGS OR EPDFCRYPT_CPPFLAGS STREQUAL "") AND
       (NOT DEFINED EPDFCRYPT_LDFLAGS OR EPDFCRYPT_LDFLAGS STREQUAL ""))
        find_path(HPDF_INCLUDE_DIR NAMES hpdf.h)
        find_library(HPDF_LIBRARY NAMES hpdf)
        if(HPDF_INCLUDE_DIR AND HPDF_LIBRARY)
            set(EPDFCRYPT_CPPFLAGS "-I${HPDF_INCLUDE_DIR}" CACHE STRING "Extra CPPFLAGS for libepdfcrypt configure" FORCE)
            get_filename_component(HPDF_LIBRARY_DIR "${HPDF_LIBRARY}" DIRECTORY)
            set(EPDFCRYPT_LDFLAGS "-L${HPDF_LIBRARY_DIR}" CACHE STRING "Extra LDFLAGS for libepdfcrypt configure" FORCE)
            message(STATUS "Found libharu at ${HPDF_LIBRARY_DIR} (passing to libepdfcrypt configure)")
        endif()
    endif()
endif()

setup_autotools_library(EPDFCRYPT libepdfcrypt epdf.hpp
    "https://github.com/drclau/libepdfcrypt" "HEAD")

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
set(SIMPLEINI_GIT_TAG "v4.25" CACHE STRING "SimpleIni git tag to fetch")
set(simpleini_local_dir "${THIRD_PARTY_DIR}/simpleini")
if(EXISTS "${simpleini_local_dir}/SimpleIni.h")
    set(simpleini_SOURCE_DIR "${simpleini_local_dir}")
    message(STATUS "Using vendored SimpleIni from ${simpleini_local_dir}")
elseif(FETCH_EXTERNAL_LIBS)
    message(STATUS "Fetching SimpleIni from https://github.com/brofield/simpleini.git (${SIMPLEINI_GIT_TAG})")
    include(FetchContent)
    FetchContent_Declare(
        simpleini
        GIT_REPOSITORY https://github.com/brofield/simpleini.git
        GIT_TAG ${SIMPLEINI_GIT_TAG}
    )
    FetchContent_MakeAvailable(simpleini)
else()
    message(FATAL_ERROR "SimpleIni not found. Options:\n"
        "  1. Place sources in ${THIRD_PARTY_DIR}/simpleini\n"
        "  2. Allow git fetch with -DFETCH_EXTERNAL_LIBS=ON")
endif()

# Toggle cfg2 demo executable (cfg2 module itself is always built as part of gwmilter)
option(ENABLE_CFG2_DEMO "Build cfg2_demo standalone executable" OFF)

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

    find_package(GTest REQUIRED)
    include(GoogleTest)

    # ============================================
    # gwmilter_tests: Unified test suite for all gwmilter components
    # ============================================
    add_executable(gwmilter_tests
        # Utils tests
        src/utils/string_tests.cpp
        src/utils/uid_generator_tests.cpp
        # Handlers tests
        src/handlers/noop_body_handler_tests.cpp
        src/handlers/body_handler_base_tests.cpp
        src/handlers/pgp_body_handler_tests.cpp
        src/handlers/smime_body_handler_tests.cpp
        src/handlers/pdf_body_handler_tests.cpp
        # cfg2 tests
        src/cfg2/deserializer_tests.cpp
        src/cfg2/dynamic_section_tests.cpp
        src/cfg2/config_tests.cpp
        src/cfg2/config_node_tests.cpp
        src/cfg2/ini_reader_tests.cpp
        src/cfg2/multiple_same_type_test.cpp
        # Source files needed for tests
        src/utils/string.cpp
        src/utils/uid_generator.cpp
        src/handlers/body_handler.cpp
        src/handlers/noop_body_handler.cpp
        src/handlers/pgp_body_handler.cpp
        src/handlers/smime_body_handler.cpp
        src/handlers/pdf_body_handler.cpp
        src/cfg2/section_registry.cpp
        src/cfg2/config.cpp
        src/cfg2/ini_reader.cpp
        src/cfg2/config_manager.cpp
    )

    target_include_directories(gwmilter_tests PRIVATE
        ${PROJECT_SOURCE_DIR}/src
        ${EGPGCRYPT_INCLUDE_DIR}
        ${EPDFCRYPT_INCLUDE_DIR}
        ${GLIB_INCLUDE_DIRS}
        ${GMIME_INCLUDE_DIRS}
        ${simpleini_SOURCE_DIR}
    )

    target_link_libraries(gwmilter_tests PRIVATE
        GTest::gtest_main
        spdlog::spdlog
        fmt::fmt
        ${EGPGCRYPT_LIBRARY}
        ${EPDFCRYPT_LIBRARY}
        PkgConfig::GLIB
        PkgConfig::GMIME
    )

    set_target_properties(gwmilter_tests PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
    )

    # Add external library dependencies if built externally
    if(USE_EGPGCRYPT_EXTERNAL)
        add_dependencies(gwmilter_tests egpgcrypt_external)
        set_target_properties(gwmilter_tests PROPERTIES
            BUILD_RPATH "${EGPGCRYPT_INSTALL_DIR}/lib")
    endif()
    if(USE_EPDFCRYPT_EXTERNAL)
        add_dependencies(gwmilter_tests epdfcrypt_external)
        get_target_property(existing_rpath gwmilter_tests BUILD_RPATH)
        if(existing_rpath)
            set_target_properties(gwmilter_tests PROPERTIES
                BUILD_RPATH "${existing_rpath}:${EPDFCRYPT_INSTALL_DIR}/lib")
        else()
            set_target_properties(gwmilter_tests PROPERTIES
                BUILD_RPATH "${EPDFCRYPT_INSTALL_DIR}/lib")
        endif()
    endif()

    # Register GoogleTest-based tests individually with CTest
    gtest_discover_tests(gwmilter_tests
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        DISCOVERY_TIMEOUT 60
    )
endif()

if(ENABLE_CFG2_DEMO)
    find_package(Threads REQUIRED)

    # Add cfg2 demo executable - standalone demonstration of cfg2 functionality
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
endif()
