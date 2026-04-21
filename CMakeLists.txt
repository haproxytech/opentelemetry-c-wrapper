## CMakeLists.txt - Miroslav Zagorac <mzagorac@haproxy.com>
##
## Alternative build system for opentelemetry-c-wrapper.
## Coexists with autotools -- no autotools files are modified.

cmake_minimum_required(VERSION 3.19)

## ---------------------------------------------------------------------------
## Read version information
## ---------------------------------------------------------------------------
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" _version_lines)
foreach(_line IN LISTS _version_lines)
    if(_line MATCHES "^Package Version:[ \t]+([0-9]+\\.[0-9]+\\.[0-9]+)")
        set(_pkg_version "${CMAKE_MATCH_1}")
    endif()
    if(_line MATCHES "^Library Version:[ \t]+([0-9]+):([0-9]+):([0-9]+)")
        set(_lt_current  "${CMAKE_MATCH_1}")
        set(_lt_revision "${CMAKE_MATCH_2}")
        set(_lt_age      "${CMAKE_MATCH_3}")
    endif()
endforeach()

if(NOT DEFINED _pkg_version)
    message(FATAL_ERROR "Failed to parse Package Version from VERSION file")
endif()
if(NOT DEFINED _lt_current)
    message(FATAL_ERROR "Failed to parse Library Version from VERSION file")
endif()

## Libtool version-info formula: SOVERSION = current - age
## Shared lib version: (current - age).age.revision
math(EXPR _so_version "${_lt_current} - ${_lt_age}")
set(_lib_version "${_so_version}.${_lt_age}.${_lt_revision}")

file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/src/.build-counter"
     PACKAGE_BUILD LIMIT_COUNT 1)
string(STRIP "${PACKAGE_BUILD}" PACKAGE_BUILD)

## ---------------------------------------------------------------------------
## Project
## ---------------------------------------------------------------------------
project(opentelemetry-c-wrapper
    VERSION   ${_pkg_version}
    LANGUAGES C CXX
)

set(CMAKE_C_STANDARD   11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(${CMAKE_SOURCE_DIR}/cmake/gen-config.cmake)
include(GNUInstallDirs)
include(CheckIncludeFile)
include(CheckFunctionExists)
include(CheckSymbolExists)

## ---------------------------------------------------------------------------
## Options
## ---------------------------------------------------------------------------
option(ENABLE_DEBUG    "Include debugging options"           OFF)
option(ENABLE_WARNINGS "Enable additional compiler warnings" OFF)
option(ENABLE_THREADS  "Enable multi-thread support"          ON)
option(WITH_LIBFYAML   "Use libfyaml YAML library"           OFF)
option(WITH_RAPIDYAML  "Use rapidyaml YAML library"           ON)

set(OPENTELEMETRY_DIR "" CACHE PATH "OpenTelemetry C++ SDK install prefix")
set(LIBFYAML_DIR      "" CACHE PATH "libfyaml install prefix")
set(RAPIDYAML_DIR     "" CACHE PATH "rapidyaml install prefix")

if(WITH_LIBFYAML AND WITH_RAPIDYAML)
    message(FATAL_ERROR
        "Options WITH_LIBFYAML and WITH_RAPIDYAML cannot be used"
        " at the same time")
endif()

## ---------------------------------------------------------------------------
## Default compiler flags (mirrors AX_PROG_CC_SET / AX_PROG_CXX_SET)
## ---------------------------------------------------------------------------
include(CheckCCompilerFlag)
include(CheckCXXCompilerFlag)

## Test a list of C flags; append supported ones to _out.
macro(check_c_flags _out)
    foreach(_flag IN ITEMS ${ARGN})
        string(MAKE_C_IDENTIFIER "CC_${_flag}" _var)
        check_c_compiler_flag("${_flag}" ${_var})
        if(${_var})
            set(${_out} "${${_out}} ${_flag}")
        endif()
    endforeach()
endmacro()

## Test a list of C++ flags; append supported ones to _out.
macro(check_cxx_flags _out)
    foreach(_flag IN ITEMS ${ARGN})
        string(MAKE_C_IDENTIFIER "CXX_${_flag}" _var)
        check_cxx_compiler_flag("${_flag}" ${_var})
        if(${_var})
            set(${_out} "${${_out}} ${_flag}")
        endif()
    endforeach()
endmacro()

## -- C flags --
set(_checked_cflags "")
check_c_flags(_checked_cflags
    -Werror=unknown-warning-option
    -Wall
    -Wextra
)
if(ENABLE_WARNINGS)
    check_c_flags(_checked_cflags
        -Walloca
        -Waddress-of-packed-member
        -Waggregate-return
        -Wbad-function-cast
        -Wcast-align
        -Wdiscarded-qualifiers
        -Werror=nonnull
        -Wfloat-equal
        -Winline
        -Wmissing-declarations
        -Wmissing-noreturn
        -Wmissing-prototypes
        -Wnested-externs
        -Wold-style-definition
        -Wpointer-arith
        -Wredundant-decls
        -Wshadow
        -Wstrict-prototypes
        -Wundef
        -Wunused
        -Wvla
        -Wwrite-strings
    )
    if(ENABLE_DEBUG)
        check_c_flags(_checked_cflags -Wformat=2)
    else()
        check_c_flags(_checked_cflags
            -Wformat-security
            -Wformat-y2k
        )
    endif()
else()
    check_c_flags(_checked_cflags -Werror)
endif()

## -- C++ flags --
set(_checked_cxxflags "")
check_cxx_flags(_checked_cxxflags
    -Werror=unknown-warning-option
    -Wall
    -Wextra
)
if(ENABLE_WARNINGS)
    check_cxx_flags(_checked_cxxflags
        -Walloca
        -Waddress-of-packed-member
        -Wcast-align
        -Werror=nonnull
        -Wfloat-equal
        -Winline
        -Wmissing-declarations
        -Wmissing-noreturn
        -Wpointer-arith
        -Wundef
        -Wunused
        -Wvla
        -Wwrite-strings
        -Wno-invalid-offsetof
        -Wno-terminate
        -Wnoexcept
        -Woverloaded-virtual
        -Wsign-promo
        -Wsized-deallocation
        -Wstrict-null-sentinel
        -Wvirtual-inheritance
    )
    if(ENABLE_DEBUG)
        check_cxx_flags(_checked_cxxflags
            -Waggregate-return
            -Wconditionally-supported
            -Wformat=2
            -Wmultiple-inheritance
            -Wnamespaces
            -Wold-style-cast
            -Wredundant-decls
            -Wshadow
            -Wtemplates
            -Wuseless-cast
            -Wzero-as-null-pointer-constant
        )
    else()
        check_cxx_flags(_checked_cxxflags
            -Wformat-security
            -Wformat-y2k
            -ftemplate-backtrace-limit=0
        )
    endif()
else()
    check_cxx_flags(_checked_cxxflags -Werror)
endif()

## -- Compose final flags --
if(ENABLE_DEBUG)
    set(_base_cflags   "-g -O0")
    set(_base_cxxflags "-g -O0")
else()
    set(_base_cflags   "-O2")
    set(_base_cxxflags "-O2")
endif()

## Apply only when the user has not overridden CFLAGS / CXXFLAGS.
if(NOT CMAKE_C_FLAGS)
    set(CMAKE_C_FLAGS "${_base_cflags}${_checked_cflags}"
        CACHE STRING "" FORCE)
endif()
if(NOT CMAKE_CXX_FLAGS)
    set(CMAKE_CXX_FLAGS "${_base_cxxflags}${_checked_cxxflags}"
        CACHE STRING "" FORCE)
endif()

## ---------------------------------------------------------------------------
## Package metadata
## ---------------------------------------------------------------------------
set(PACKAGE           "${PROJECT_NAME}")
set(PACKAGE_NAME      "${PROJECT_NAME}")
set(PACKAGE_VERSION   "${PROJECT_VERSION}")
set(PACKAGE_STRING    "${PROJECT_NAME} ${PROJECT_VERSION}")
set(PACKAGE_TARNAME   "${PROJECT_NAME}")
set(PACKAGE_BUGREPORT "mzagorac@haproxy.com")
set(PACKAGE_URL       "https://github.com/haproxytech/opentelemetry-c-wrapper")
set(VERSION           "${PROJECT_VERSION}")
set(LIB_VERSION       "${_lt_current}:${_lt_revision}:${_lt_age}")

## ---------------------------------------------------------------------------
## Header / function checks
## ---------------------------------------------------------------------------
check_include_file(dlfcn.h     HAVE_DLFCN_H)
check_include_file(fcntl.h     HAVE_FCNTL_H)
check_include_file(inttypes.h  HAVE_INTTYPES_H)
check_include_file(malloc.h    HAVE_MALLOC_H)
check_include_file(sched.h     HAVE_SCHED_H)
check_include_file(stdint.h    HAVE_STDINT_H)
check_include_file(stdio.h     HAVE_STDIO_H)
check_include_file(stdlib.h    HAVE_STDLIB_H)
check_include_file(string.h    HAVE_STRING_H)
check_include_file(strings.h   HAVE_STRINGS_H)
check_include_file(sys/stat.h  HAVE_SYS_STAT_H)
check_include_file(sys/time.h  HAVE_SYS_TIME_H)
check_include_file(sys/types.h HAVE_SYS_TYPES_H)
check_include_file(unistd.h    HAVE_UNISTD_H)
check_include_file(wchar.h     HAVE_WCHAR_H)

## check_symbol_exists needs _GNU_SOURCE for strndup visibility.
set(CMAKE_REQUIRED_DEFINITIONS -D_GNU_SOURCE)

check_function_exists(clock_gettime HAVE_CLOCK_GETTIME)
check_function_exists(gettimeofday  HAVE_GETTIMEOFDAY)
check_symbol_exists(mallinfo   "malloc.h"  HAVE_MALLINFO)
check_symbol_exists(mallinfo2  "malloc.h"  HAVE_MALLINFO2)
check_symbol_exists(memset     "string.h"  HAVE_MEMSET)
check_symbol_exists(strcasecmp "strings.h" HAVE_STRCASECMP)
check_symbol_exists(strndup    "string.h"  HAVE_STRNDUP)
check_symbol_exists(strrchr    "string.h"  HAVE_STRRCHR)
check_symbol_exists(strtol     "stdlib.h"  HAVE_STRTOL)

unset(CMAKE_REQUIRED_DEFINITIONS)

check_symbol_exists(strerror_r "string.h" HAVE_STRERROR_R)
if(HAVE_STRERROR_R)
    set(HAVE_DECL_STRERROR_R 1)
    include(CheckCSourceCompiles)
    check_c_source_compiles("
        #define _GNU_SOURCE
        #include <string.h>
        int main(void) { char buf[64]; char *p = strerror_r(0, buf, sizeof(buf)); (void)p; return 0; }
    " STRERROR_R_CHAR_P)
endif()

set(HAVE_MALLOC  1)
set(HAVE_REALLOC 1)
set(STDC_HEADERS 1)
set(_GNU_SOURCE  1)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(LINUX 1)
endif()

## ---------------------------------------------------------------------------
## OpenTelemetry C++ SDK  (via pkg-config)
## ---------------------------------------------------------------------------
find_package(PkgConfig REQUIRED)

if(OPENTELEMETRY_DIR)
    set(ENV{PKG_CONFIG_PATH}
        "${OPENTELEMETRY_DIR}/lib/pkgconfig:${OPENTELEMETRY_DIR}/lib64/pkgconfig:$ENV{PKG_CONFIG_PATH}")
endif()

pkg_check_modules(OPENTELEMETRY REQUIRED opentelemetry_api>=1.26.0)

## Add -DOPENTELEMETRY_PROTO_API= to mirror autotools.
list(APPEND OPENTELEMETRY_CFLAGS_OTHER "-DOPENTELEMETRY_PROTO_API=")

## Find the OpenTelemetry include directory for version extraction.
set(_otel_includedir "")
if(OPENTELEMETRY_INCLUDEDIR)
    set(_otel_includedir "${OPENTELEMETRY_INCLUDEDIR}")
elseif(OPENTELEMETRY_INCLUDE_DIRS)
    list(GET OPENTELEMETRY_INCLUDE_DIRS 0 _otel_includedir)
else()
    ## Fallback: ask pkg-config directly.
    execute_process(
        COMMAND pkg-config --variable=includedir opentelemetry_api
        OUTPUT_VARIABLE _otel_includedir
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
endif()

## Extract OTELCPP_VERSION from opentelemetry/version.h
set(OTELCPP_VERSION "unknown")
if(_otel_includedir AND EXISTS "${_otel_includedir}/opentelemetry/version.h")
    file(STRINGS "${_otel_includedir}/opentelemetry/version.h" _otel_ver_line
         REGEX "^#define OPENTELEMETRY_VERSION ")
    if(_otel_ver_line MATCHES "\"([^\"]+)\"")
        set(OTELCPP_VERSION "${CMAKE_MATCH_1}")
    endif()
endif()
message(STATUS "OpenTelemetry C++ SDK version: ${OTELCPP_VERSION}")

## Verify ABI version is 2.
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/_abi_check.cpp" [=[
#include <stdio.h>
#include <opentelemetry/version.h>
int main(void) { printf("%d", OPENTELEMETRY_ABI_VERSION_NO); return 0; }
]=])
try_run(_abi_run_rc _abi_compile_rc
    "${CMAKE_CURRENT_BINARY_DIR}"
    "${CMAKE_CURRENT_BINARY_DIR}/_abi_check.cpp"
    CMAKE_FLAGS "-DINCLUDE_DIRECTORIES:STRING=${_otel_includedir}"
    COMPILE_DEFINITIONS ${OPENTELEMETRY_CFLAGS_OTHER}
    RUN_OUTPUT_VARIABLE _otel_abi_version
)
if(NOT _abi_compile_rc)
    message(FATAL_ERROR "Failed to compile OpenTelemetry ABI version check")
endif()
string(STRIP "${_otel_abi_version}" _otel_abi_version)
if(NOT _otel_abi_version STREQUAL "2")
    message(FATAL_ERROR
        "Invalid ABI version: expected 2, got ${_otel_abi_version}")
endif()
message(STATUS "OpenTelemetry ABI version: ${_otel_abi_version}")

## ---------------------------------------------------------------------------
## Exporter probing  (find_library based)
## ---------------------------------------------------------------------------
## Collect extra library search paths from the OpenTelemetry pkg-config.
set(_otel_lib_dirs "")
foreach(_dir IN LISTS OPENTELEMETRY_LIBRARY_DIRS)
    list(APPEND _otel_lib_dirs "${_dir}")
endforeach()
if(OPENTELEMETRY_DIR)
    list(APPEND _otel_lib_dirs
        "${OPENTELEMETRY_DIR}/lib" "${OPENTELEMETRY_DIR}/lib64")
endif()

set(EXPORTER_LIBRARIES "")

macro(probe_exporter _name)
    set(_found TRUE)
    set(_libs "")
    foreach(_lib IN ITEMS ${ARGN})
        find_library(_loc_${_lib} NAMES ${_lib}
            HINTS ${_otel_lib_dirs}
            NO_DEFAULT_PATH
        )
        find_library(_loc_${_lib} NAMES ${_lib})
        if(_loc_${_lib})
            list(APPEND _libs "${_loc_${_lib}}")
        else()
            set(_found FALSE)
        endif()
    endforeach()
    if(_found)
        set(HAVE_OTEL_EXPORTER_${_name} 1)
        list(APPEND EXPORTER_LIBRARIES ${_libs})
        message(STATUS "  ${_name} exporter: found")
    else()
        message(STATUS "  ${_name} exporter: not found")
    endif()
endmacro()

message(STATUS "Probing OpenTelemetry exporters:")

probe_exporter(ELASTICSEARCH
    opentelemetry_exporter_elasticsearch_logs)
probe_exporter(IN_MEMORY
    opentelemetry_exporter_in_memory
    opentelemetry_exporter_in_memory_metric)
probe_exporter(OSTREAM
    opentelemetry_exporter_ostream_span
    opentelemetry_exporter_ostream_metrics
    opentelemetry_exporter_ostream_logs)
probe_exporter(OTLP_FILE
    opentelemetry_exporter_otlp_file
    opentelemetry_exporter_otlp_file_metric
    opentelemetry_exporter_otlp_file_log)
probe_exporter(OTLP_GRPC
    opentelemetry_exporter_otlp_grpc
    opentelemetry_exporter_otlp_grpc_metrics
    opentelemetry_exporter_otlp_grpc_log)
probe_exporter(OTLP_HTTP
    opentelemetry_exporter_otlp_http
    opentelemetry_exporter_otlp_http_metric
    opentelemetry_exporter_otlp_http_log)
probe_exporter(ZIPKIN
    opentelemetry_exporter_zipkin_trace)

## ---------------------------------------------------------------------------
## YAML backend
## ---------------------------------------------------------------------------
set(YAML_INCLUDE_DIRS "")
set(YAML_LIBRARIES    "")
set(YAML_LIBRARY_DIRS "")

if(WITH_LIBFYAML)
    if(LIBFYAML_DIR)
        set(ENV{PKG_CONFIG_PATH}
            "${LIBFYAML_DIR}/lib/pkgconfig:${LIBFYAML_DIR}/lib64/pkgconfig:$ENV{PKG_CONFIG_PATH}")
    endif()
    pkg_check_modules(LIBFYAML REQUIRED libfyaml)
    set(HAVE_LIBFYAML_H 1)
    set(YAML_INCLUDE_DIRS "${LIBFYAML_INCLUDE_DIRS}")
    set(YAML_LIBRARIES    "${LIBFYAML_LIBRARIES}")
    set(YAML_LIBRARY_DIRS "${LIBFYAML_LIBRARY_DIRS}")
    message(STATUS "YAML backend: libfyaml")
elseif(WITH_RAPIDYAML)
    set(_ryml_search_include "")
    set(_ryml_search_lib     "")
    if(RAPIDYAML_DIR)
        set(_ryml_search_include "${RAPIDYAML_DIR}/include")
        set(_ryml_search_lib     "${RAPIDYAML_DIR}/lib"
                                 "${RAPIDYAML_DIR}/lib64")
    elseif(CMAKE_INSTALL_PREFIX)
        set(_ryml_search_include "${CMAKE_INSTALL_PREFIX}/include")
        set(_ryml_search_lib     "${CMAKE_INSTALL_PREFIX}/lib"
                                 "${CMAKE_INSTALL_PREFIX}/lib64")
    endif()

    find_path(RYML_INCLUDE_DIR ryml_std.hpp
        HINTS ${_ryml_search_include}
    )
    find_library(RYML_LIBRARY NAMES ryml
        HINTS ${_ryml_search_lib}
    )

    if(NOT RYML_INCLUDE_DIR OR NOT RYML_LIBRARY)
        message(FATAL_ERROR "rapidyaml not found (ryml_std.hpp / libryml)")
    endif()

    set(HAVE_RYML_STD_HPP 1)
    set(YAML_INCLUDE_DIRS "${RYML_INCLUDE_DIR}")
    set(YAML_LIBRARIES    "${RYML_LIBRARY}")
    message(STATUS "YAML backend: rapidyaml")
else()
    message(STATUS "YAML backend: none")
endif()

## ---------------------------------------------------------------------------
## Threads
## ---------------------------------------------------------------------------
if(ENABLE_THREADS)
    find_package(Threads REQUIRED)
    set(USE_THREADS  1)
    set(_REENTRANT   1)
    set(HAVE_LIBPTHREAD 1)
endif()

## ---------------------------------------------------------------------------
## Debug mode
## ---------------------------------------------------------------------------
if(ENABLE_DEBUG)
    set(DEBUG 1)
    set(_lib_name      "opentelemetry-c-wrapper_dbg")
    set(_test_bin_name "otel-c-wrapper-test_dbg")
    set(_version_map   "${CMAKE_CURRENT_SOURCE_DIR}/src/export_dbg.map")
    set(_pc_template   "opentelemetry-c-wrapper_dbg.pc.in")
    set(_pc_output     "opentelemetry-c-wrapper_dbg.pc")
else()
    set(_lib_name      "opentelemetry-c-wrapper")
    set(_test_bin_name "otel-c-wrapper-test")
    set(_version_map   "${CMAKE_CURRENT_SOURCE_DIR}/src/export.map")
    set(_pc_template   "opentelemetry-c-wrapper.pc.in")
    set(_pc_output     "opentelemetry-c-wrapper.pc")
endif()

## ---------------------------------------------------------------------------
## Generate version.h
## ---------------------------------------------------------------------------
## Substitution variables for version.h.in
string(TIMESTAMP DATE "%Y/%m/%d")
set(DEFS              "")
set(CPPFLAGS          "")
set(CFLAGS            "${CMAKE_C_FLAGS}")
set(CXXFLAGS          "${CMAKE_CXX_FLAGS}")
set(CONFIGURE_OPTIONS "cmake")

file(MAKE_DIRECTORY
    "${CMAKE_CURRENT_BINARY_DIR}/include/opentelemetry-c-wrapper")

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/include/opentelemetry-c-wrapper/version.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/include/opentelemetry-c-wrapper/version.h"
    @ONLY
)

## Generate a build-time cmake script that re-creates version.h with the
## updated build counter.  All substitution variables except PACKAGE_BUILD
## are baked in at configure time; PACKAGE_BUILD is read from the counter
## file at build time.
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/regen-version-h.cmake" "\
file(STRINGS \"${CMAKE_CURRENT_SOURCE_DIR}/src/.build-counter\" PACKAGE_BUILD LIMIT_COUNT 1)\n\
string(STRIP \"\${PACKAGE_BUILD}\" PACKAGE_BUILD)\n\
set(PACKAGE_BUGREPORT \"${PACKAGE_BUGREPORT}\")\n\
set(PACKAGE_URL       \"${PACKAGE_URL}\")\n\
set(PACKAGE_VERSION   \"${PACKAGE_VERSION}\")\n\
set(DATE              \"${DATE}\")\n\
set(DEFS              \"${DEFS}\")\n\
set(CPPFLAGS          \"${CPPFLAGS}\")\n\
set(CFLAGS            \"${CFLAGS}\")\n\
set(CXXFLAGS          \"${CXXFLAGS}\")\n\
set(CONFIGURE_OPTIONS \"${CONFIGURE_OPTIONS}\")\n\
set(OTELCPP_VERSION   \"${OTELCPP_VERSION}\")\n\
configure_file(\n\
    \"${CMAKE_CURRENT_SOURCE_DIR}/include/opentelemetry-c-wrapper/version.h.in\"\n\
    \"${CMAKE_CURRENT_BINARY_DIR}/include/opentelemetry-c-wrapper/version.h\"\n\
    @ONLY)\n\
")

## ---------------------------------------------------------------------------
## Generate otel_cpp.h from otel_cpp.h.in
## ---------------------------------------------------------------------------
## Span kind enum -- extracted from trace.pb.h (mirrors AX_OTEL_CPP_HDR_SPAN_KIND).
set(_trace_pb_h
    "${_otel_includedir}/opentelemetry/proto/trace/v1/trace.pb.h")

if(EXISTS "${_trace_pb_h}")
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/_span_kind.awk" [=[
BEGIN { print "typedef enum {" }
/^ *Span_SpanKind_SPAN_KIND_/ { sub(/Span_SpanKind/,"OTELC",$0); print "\t"$1" "$2" "$3 }
END { print "} otelc_span_kind_t;" }
]=])
    execute_process(
        COMMAND awk -f "${CMAKE_CURRENT_BINARY_DIR}/_span_kind.awk" "${_trace_pb_h}"
        OUTPUT_VARIABLE OTEL_CPP_HDR_SPAN_KIND
        OUTPUT_STRIP_TRAILING_WHITESPACE
        COMMAND_ERROR_IS_FATAL ANY
    )
else()
    message(WARNING "trace.pb.h not found; span kind enum unavailable")
    set(OTEL_CPP_HDR_SPAN_KIND "")
endif()

## Semantic conventions -- generated by scripts/semconv.sh.
execute_process(
    COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/scripts/semconv.sh" "${_otel_includedir}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    RESULT_VARIABLE _semconv_rc
)
if(NOT _semconv_rc EQUAL 0)
    message(FATAL_ERROR "scripts/semconv.sh failed (rc=${_semconv_rc})")
endif()

set(_semconv_h "${CMAKE_CURRENT_SOURCE_DIR}/scripts/semconv.h")
if(NOT EXISTS "${_semconv_h}")
    message(FATAL_ERROR
        "scripts/semconv.sh did not produce scripts/semconv.h")
endif()

## Extract only #define lines (skip copyright / include guards).
execute_process(
    COMMAND grep "^#define [a-z]" "${_semconv_h}"
    OUTPUT_VARIABLE OTEL_CPP_HDR_SEMANTIC_CONVENTIONS
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/include/opentelemetry-c-wrapper/otel_cpp.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/include/opentelemetry-c-wrapper/otel_cpp.h"
    @ONLY
)

## ---------------------------------------------------------------------------
## pkg-config .pc file
## ---------------------------------------------------------------------------
set(prefix      "${CMAKE_INSTALL_PREFIX}")
set(exec_prefix "${CMAKE_INSTALL_PREFIX}")
set(libdir      "${CMAKE_INSTALL_FULL_LIBDIR}")
set(includedir  "${CMAKE_INSTALL_FULL_INCLUDEDIR}")

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/${_pc_template}"
    "${CMAKE_CURRENT_BINARY_DIR}/${_pc_output}"
    @ONLY
)

## ---------------------------------------------------------------------------
## Library sources
## ---------------------------------------------------------------------------
set(_lib_sources
    src/exporter.cpp
    src/logger.cpp
    src/meter.cpp
    src/processor.cpp
    src/provider.cpp
    src/resource.cpp
    src/sampler.cpp
    src/span.cpp
    src/threads.cpp
    src/tracer.cpp
    src/util.cpp
    src/yaml.cpp
    src/version.cpp
)

if(ENABLE_DEBUG)
    list(APPEND _lib_sources src/dbg_malloc.cpp)
endif()

## ---------------------------------------------------------------------------
## Build counter  (mirrors autotools BUILT_SOURCES)
## ---------------------------------------------------------------------------
## Run scripts/build-counter before compiling so the counter is incremented
## when source files change, matching the autotools behaviour.  Then
## regenerate version.h so that OTELC_PACKAGE_BUILD reflects the new value.
add_custom_target(build-counter
    COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/scripts/build-counter"
    COMMAND "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/regen-version-h.cmake"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "Updating build counter"
)

## ---------------------------------------------------------------------------
## Library target
## ---------------------------------------------------------------------------
add_library(${_lib_name} SHARED ${_lib_sources})
add_dependencies(${_lib_name} build-counter)

set_target_properties(${_lib_name} PROPERTIES
    VERSION   ${_lib_version}
    SOVERSION ${_so_version}
    LINK_FLAGS "-Wl,--version-script=${_version_map} -Wl,-z,nodelete"
)

target_include_directories(${_lib_name}
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        "${CMAKE_CURRENT_BINARY_DIR}"
        ${OPENTELEMETRY_INCLUDE_DIRS}
        ${YAML_INCLUDE_DIRS}
)

target_compile_definitions(${_lib_name} PRIVATE
    HAVE_CONFIG_H
    ${OPENTELEMETRY_CFLAGS_OTHER}
)

if(ENABLE_DEBUG)
    target_compile_definitions(${_lib_name}
        PRIVATE   DEBUG
        INTERFACE $<INSTALL_INTERFACE:OTELC_DBG_MEM>
    )
endif()

target_link_directories(${_lib_name} PRIVATE
    ${OPENTELEMETRY_LIBRARY_DIRS}
    ${YAML_LIBRARY_DIRS}
)

target_link_libraries(${_lib_name} PRIVATE
    ${OPENTELEMETRY_LIBRARIES}
    ${EXPORTER_LIBRARIES}
    ${YAML_LIBRARIES}
)

if(ENABLE_THREADS)
    target_link_libraries(${_lib_name} PRIVATE Threads::Threads)
endif()

## ---------------------------------------------------------------------------
## Common settings for test / tool targets
## ---------------------------------------------------------------------------
## Helper: set up an executable that links the wrapper library.
function(add_wrapper_executable _target)
    cmake_parse_arguments(_ARG "CXX" "" "SOURCES" ${ARGN})

    add_executable(${_target} EXCLUDE_FROM_ALL ${_ARG_SOURCES})

    target_include_directories(${_target} PRIVATE
        "${CMAKE_CURRENT_BINARY_DIR}"
        "${CMAKE_CURRENT_BINARY_DIR}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
    )

    target_compile_definitions(${_target} PRIVATE
        HAVE_CONFIG_H
    )

    target_link_libraries(${_target} PRIVATE ${_lib_name})

    if(ENABLE_THREADS)
        target_link_libraries(${_target} PRIVATE Threads::Threads)
    endif()

    ## Extra include paths for C++ test that needs YAML / OTel headers.
    if(_ARG_CXX)
        target_include_directories(${_target} PRIVATE
            ${OPENTELEMETRY_INCLUDE_DIRS}
            ${YAML_INCLUDE_DIRS}
        )
        target_compile_definitions(${_target} PRIVATE
            ${OPENTELEMETRY_CFLAGS_OTHER}
        )
        target_link_directories(${_target} PRIVATE
            ${OPENTELEMETRY_LIBRARY_DIRS}
            ${YAML_LIBRARY_DIRS}
        )
    endif()
endfunction()

## ---------------------------------------------------------------------------
## Test / tool targets
## ---------------------------------------------------------------------------
## otel-c-wrapper-test  (installed)
add_wrapper_executable(${_test_bin_name}
    SOURCES test/test.c test/opentelemetry.c)
target_link_libraries(${_test_bin_name} PRIVATE m)

## Non-installed test binaries carry a _dbg suffix in debug mode.
if(ENABLE_DEBUG)
    set(_test_suffix "_dbg")
else()
    set(_test_suffix "")
endif()

## test-tracer  (not installed)
add_wrapper_executable(test-tracer${_test_suffix}
    SOURCES test/test-tracer.c test/test-util.c test/opentelemetry.c)

## test-meter  (not installed)
add_wrapper_executable(test-meter${_test_suffix}
    SOURCES test/test-meter.c test/test-util.c)

## test-logger  (not installed)
add_wrapper_executable(test-logger${_test_suffix}
    SOURCES test/test-logger.c test/test-util.c)

## test-yaml  (C++ -- not installed)
add_wrapper_executable(test-yaml${_test_suffix} CXX
    SOURCES test/test-yaml.cpp test/test-util.c)

## Copy YAML config files to the build directory so test programs can find them
file(GLOB _test_yml_files "${CMAKE_CURRENT_SOURCE_DIR}/test/*.yml")
foreach(_yml IN LISTS _test_yml_files)
    configure_file("${_yml}" "${CMAKE_CURRENT_BINARY_DIR}" COPYONLY)
endforeach()

## Custom "test" target -- builds all test programs on demand.
add_custom_target(test DEPENDS
    ${_test_bin_name}
    test-tracer${_test_suffix}
    test-meter${_test_suffix}
    test-logger${_test_suffix}
    test-yaml${_test_suffix}
)

## ---------------------------------------------------------------------------
## Installation RPATH
## ---------------------------------------------------------------------------
set_target_properties(${_lib_name} PROPERTIES
    INSTALL_RPATH "${CMAKE_INSTALL_FULL_LIBDIR}"
)
set_target_properties(${_test_bin_name} PROPERTIES
    INSTALL_RPATH "${CMAKE_INSTALL_FULL_LIBDIR}"
)

## ---------------------------------------------------------------------------
## Installation
## ---------------------------------------------------------------------------
## Library
install(TARGETS ${_lib_name}
    EXPORT ${_lib_name}Targets
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
)

## CMake export file
install(EXPORT ${_lib_name}Targets
    NAMESPACE ${_lib_name}::
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${_lib_name}"
)

## Public headers
set(_public_headers
    include/opentelemetry-c-wrapper/dbg_malloc.h
    include/opentelemetry-c-wrapper/debug.h
    include/opentelemetry-c-wrapper/define.h
    include/opentelemetry-c-wrapper/include.h
    include/opentelemetry-c-wrapper/logger.h
    include/opentelemetry-c-wrapper/meter.h
    include/opentelemetry-c-wrapper/propagation.h
    include/opentelemetry-c-wrapper/span.h
    include/opentelemetry-c-wrapper/tracer.h
    include/opentelemetry-c-wrapper/util.h
)
install(FILES ${_public_headers}
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/opentelemetry-c-wrapper"
)
## Generated headers (version.h, otel_cpp.h)
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/include/opentelemetry-c-wrapper/version.h"
    "${CMAKE_CURRENT_BINARY_DIR}/include/opentelemetry-c-wrapper/otel_cpp.h"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/opentelemetry-c-wrapper"
)

## Installed test binary
install(TARGETS ${_test_bin_name}
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" OPTIONAL
)

## pkg-config
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/${_pc_output}"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig"
)

## CMake package config
include(CMakePackageConfigHelpers)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Config.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/${_lib_name}Config.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${_lib_name}"
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/${_lib_name}ConfigVersion.cmake"
    VERSION "${PROJECT_VERSION}"
    COMPATIBILITY SameMajorVersion
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/${_lib_name}Config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/${_lib_name}ConfigVersion.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${_lib_name}"
)

## Documentation
install(FILES
    AUTHORS COPYING ChangeLog INSTALL LICENSE MAINTAINERS NEWS README TODO VERSION
    DESTINATION "${CMAKE_INSTALL_DOCDIR}"
)

## YAML test configuration
file(GLOB _install_yml_files "${CMAKE_CURRENT_SOURCE_DIR}/test/*.yml")
install(FILES ${_install_yml_files}
    DESTINATION "${CMAKE_INSTALL_DOCDIR}"
)

# Auto-generate config.h from CMakeLists.txt
generate_config_from_cmakefile()

## ---------------------------------------------------------------------------
## Summary
## ---------------------------------------------------------------------------
message(STATUS "")
message(STATUS "=== ${PROJECT_NAME} ${PROJECT_VERSION} (build ${PACKAGE_BUILD}) ===")
message(STATUS "  Library version : ${LIB_VERSION}  (SO ${_so_version}, file ${_lib_version})")
message(STATUS "  OTel C++ SDK    : ${OTELCPP_VERSION}")
message(STATUS "  Debug           : ${ENABLE_DEBUG}")
message(STATUS "  Threads         : ${ENABLE_THREADS}")
message(STATUS "  YAML backend    : ${WITH_LIBFYAML}(libfyaml) / ${WITH_RAPIDYAML}(rapidyaml)")
message(STATUS "  Install prefix  : ${CMAKE_INSTALL_PREFIX}")
message(STATUS "")
