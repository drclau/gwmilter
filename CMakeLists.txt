cmake_minimum_required(VERSION 3.13.4)

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

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
endif()

# Find necessary packages
find_package(PkgConfig REQUIRED)
find_package(CURL REQUIRED)
find_package(Boost 1.52.0 REQUIRED COMPONENTS regex)
find_package(fmt REQUIRED)

# Use pkg-config to find and configure GLib and GMime
pkg_check_modules(GLIB REQUIRED IMPORTED_TARGET glib-2.0)
pkg_check_modules(GMIME REQUIRED IMPORTED_TARGET gmime-3.0)

# Find paths and libraries for libmilter
find_path(MILTER_INCLUDE_DIR NAMES libmilter/mfapi.h)
find_library(MILTER_LIBRARY NAMES milter)

# Find paths and libraries for EGPGCRYPT and EPDFCRYPT
set(EGPGCRYPT_PATH "/usr/local" CACHE PATH "Path to libegpgcrypt installation")
set(EPDFCRYPT_PATH "/usr/local" CACHE PATH "Path to libepdfcrypt installation")

find_path(EGPGCRYPT_INCLUDE_DIR NAMES crypto.hpp HINTS ${EGPGCRYPT_PATH}/include)
find_library(EGPGCRYPT_LIBRARY NAMES egpgcrypt HINTS ${EGPGCRYPT_PATH}/lib)
find_path(EPDFCRYPT_INCLUDE_DIR NAMES epdf.hpp HINTS ${EPDFCRYPT_PATH}/include)
find_library(EPDFCRYPT_LIBRARY NAMES epdfcrypt HINTS ${EPDFCRYPT_PATH}/lib)

# Define the executable and its source files
add_executable(gwmilter
        src/main.cpp
        src/cfg/cfg.hpp
        src/cfg/cfg.cpp
        src/handlers/body_handler.hpp
        src/handlers/body_handler.cpp
        src/handlers/headers.hpp
        src/handlers/noop_body_handler.cpp
        src/handlers/pdf_body_handler.cpp
        src/handlers/pgp_body_handler.cpp
        src/handlers/smime_body_handler.cpp
        src/logger/logger.hpp
        src/logger/logger.cpp
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
)

# Set include directories for gwmilter target
target_include_directories(gwmilter PRIVATE
        ${PROJECT_SOURCE_DIR}/src
        ${CURL_INCLUDE_DIRS}
        ${MILTER_INCLUDE_DIR}
        ${Boost_INCLUDE_DIRS}
        ${GLIB_INCLUDE_DIRS}
        ${GMIME_INCLUDE_DIRS}
        ${EGPGCRYPT_INCLUDE_DIR}
        ${EPDFCRYPT_INCLUDE_DIR}
        ${FMT_INCLUDE_DIRS}
)

# Link libraries to gwmilter target
target_link_libraries(gwmilter PRIVATE
        CURL::libcurl
        fmt::fmt
        ${MILTER_LIBRARY}
        Boost::regex
        PkgConfig::GLIB
        PkgConfig::GMIME
        ${EGPGCRYPT_LIBRARY}
        ${EPDFCRYPT_LIBRARY}
)

# std::filesystem requires linking against a separate library on older compilers
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.1)
    target_link_libraries(gwmilter PRIVATE stdc++fs)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.0)
    target_link_libraries(gwmilter PRIVATE c++fs)
endif()
