# DataFrame module CMake configuration
#
# Copyright (C) 2026 Qore Technologies, s.r.o.
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# DEALINGS IN THE SOFTWARE.

# Eigen is required (same discovery as ml module)
find_package(Eigen3 3.4 QUIET NO_MODULE)
if (NOT Eigen3_FOUND)
    find_package(Eigen3 REQUIRED NO_MODULE)
endif()

# Apache Arrow/Parquet is required for DataFrame Parquet I/O.
#
# Try the system-installed package first (libarrow-dev / apache-arrow / homebrew
# apache-arrow); fall back to a static build from the upstream release tarball
# via FetchContent when it is not available.  The fallback adds several minutes
# to the configure/build, so it is intentionally a fallback rather than the
# default.
#
# Selected libraries are exported through QORE_ARROW_TARGETS so the
# target_link_libraries() call below stays platform-agnostic.
find_package(Arrow QUIET CONFIG)
if (Arrow_FOUND)
    find_package(Parquet QUIET CONFIG)
endif()

if (Arrow_FOUND AND Parquet_FOUND)
    message(STATUS "Found system Apache Arrow ${Arrow_VERSION} / Parquet ${PARQUET_VERSION}")
    set(QORE_ARROW_TARGETS Arrow::arrow_shared Parquet::parquet_shared)
else()
    # FetchContent fallback: build apache-arrow + parquet from source.  The
    # release tarball ships the C++ tree under cpp/CMakeLists.txt; we point
    # FetchContent at that subdirectory so the rest of the monorepo (Java,
    # Python, R, Go, ...) is not configured.  We force static builds so the
    # dataframe module does not need libarrow.so/dylib at runtime, and bundle
    # Arrow's own dependencies (Snappy/LZ4/ZSTD/Thrift/...) so the build does
    # not require additional system packages.
    set(QORE_ARROW_FETCH_VERSION "24.0.0")
    message(STATUS "Apache Arrow not installed; building from source via FetchContent (apache-arrow ${QORE_ARROW_FETCH_VERSION})")
    message(STATUS "  This adds ~5-15 minutes to the build.  Install libarrow-dev / apache-arrow to skip.")

    include(FetchContent)

    # Save cache variables we override for the subproject build.  The pattern
    # mirrors the nghttp2 / ngtcp2 / nghttp3 FetchContent setup in the top-level
    # CMakeLists.txt — see comments there for why per-var save/restore is used
    # instead of CMAKE_CACHE_DEFAULT_ARGS.
    set(_SAVE_AR_BUILD_TESTING ${BUILD_TESTING})
    set(_SAVE_AR_BUILD_SHARED_LIBS ${BUILD_SHARED_LIBS})
    set(_SAVE_AR_PIC ${CMAKE_POSITION_INDEPENDENT_CODE})

    set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    # Arrow's static target gets linked into a shared module (dataframe.so /
    # .dylib), so it must be position-independent.
    set(CMAKE_POSITION_INDEPENDENT_CODE ON CACHE BOOL "" FORCE)

    set(ARROW_BUILD_SHARED OFF CACHE BOOL "" FORCE)
    set(ARROW_BUILD_STATIC ON CACHE BOOL "" FORCE)
    set(ARROW_PARQUET ON CACHE BOOL "" FORCE)
    set(ARROW_IPC ON CACHE BOOL "" FORCE)
    set(ARROW_CSV ON CACHE BOOL "" FORCE)
    set(ARROW_FILESYSTEM ON CACHE BOOL "" FORCE)
    # AUTO: prefer system packages for each individual third-party dep and
    # fall back to bundling only when absent.  Required (instead of BUNDLED)
    # because the parent CMakeLists.txt already imports ZLIB::ZLIB, zstd::*,
    # LZ4 and BZip2 targets — switching Arrow to "bundle everything" produces
    # "target X already exists" errors when Arrow's ThirdpartyToolchain.cmake
    # re-declares them.
    set(ARROW_DEPENDENCY_SOURCE AUTO CACHE STRING "" FORCE)
    # Common Parquet compression codecs.  Arrow turns these into Snappy/LZ4/
    # ZSTD support; leaving them OFF would produce a parquet writer that
    # cannot read most real-world files.
    set(ARROW_WITH_SNAPPY ON CACHE BOOL "" FORCE)
    set(ARROW_WITH_LZ4 ON CACHE BOOL "" FORCE)
    set(ARROW_WITH_ZSTD ON CACHE BOOL "" FORCE)
    set(ARROW_WITH_BROTLI ON CACHE BOOL "" FORCE)
    # Skip everything we do not use: avoids pulling in extra deps + build time.
    set(ARROW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(ARROW_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)
    set(ARROW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(ARROW_BUILD_UTILITIES OFF CACHE BOOL "" FORCE)
    set(ARROW_BUILD_INTEGRATION OFF CACHE BOOL "" FORCE)
    set(PARQUET_BUILD_EXECUTABLES OFF CACHE BOOL "" FORCE)
    set(PARQUET_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(arrow
        URL "https://archive.apache.org/dist/arrow/arrow-${QORE_ARROW_FETCH_VERSION}/apache-arrow-${QORE_ARROW_FETCH_VERSION}.tar.gz"
        URL_HASH SHA256=9a8094d24fa33b90c672ab77fdda253f29300c8b0dd3f0b8e55a29dbd98b82c9
        SOURCE_SUBDIR cpp
        ${QORE_FETCHCONTENT_URL_OPTIONS}
        ${QORE_FETCHCONTENT_EXCLUDE_FROM_ALL}
    )
    FetchContent_MakeAvailable(arrow)

    # Restore cache variables
    if (DEFINED _SAVE_AR_BUILD_TESTING)
        set(BUILD_TESTING ${_SAVE_AR_BUILD_TESTING} CACHE BOOL "" FORCE)
    else()
        unset(BUILD_TESTING CACHE)
    endif()
    if (DEFINED _SAVE_AR_BUILD_SHARED_LIBS)
        set(BUILD_SHARED_LIBS ${_SAVE_AR_BUILD_SHARED_LIBS} CACHE BOOL "" FORCE)
    else()
        unset(BUILD_SHARED_LIBS CACHE)
    endif()
    if (DEFINED _SAVE_AR_PIC)
        set(CMAKE_POSITION_INDEPENDENT_CODE ${_SAVE_AR_PIC} CACHE BOOL "" FORCE)
    else()
        unset(CMAKE_POSITION_INDEPENDENT_CODE CACHE)
    endif()

    # Arrow's static-build targets are named arrow_static / parquet_static when
    # ARROW_BUILD_STATIC=ON.  Public headers live under
    # ${arrow_SOURCE_DIR}/cpp/src and generated headers under
    # ${arrow_BINARY_DIR}/src.
    set(QORE_ARROW_TARGETS arrow_static parquet_static)
    message(STATUS "Apache Arrow ${QORE_ARROW_FETCH_VERSION} fetched and configured")
endif()

# Start of hacks to simulate installed QoreConfig.cmake
if (NOT QORE_MODULES_DIR)
    set(QORE_MODULES_DIR ${MODULE_DIR})
endif()

if (NOT QORE_LIBRARY)
    set(QORE_LIBRARY libqore)
endif()

set(QORE_API_VERSION ${MODULE_API_MAJOR}.${MODULE_API_MINOR})

include(${CMAKE_SOURCE_DIR}/cmake/QoreMacros.cmake)
### end of hacks

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_BINARY_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/include)

# QPP source files
set(DF_QPP_SRC
    src/QC_DataFrame.qpp
    src/QC_DataFrameExpr.qpp
    src/QC_GroupedDataFrame.qpp
    src/ql_dataframe.qpp
)

# C++ source files
set(DF_CPP_SRC
    src/dataframe-module.cpp
    src/DataFrameExpr.cpp
    src/df_plugin.cpp
    src/df_column.cpp
    src/DataFrame.cpp
    src/GroupedDataFrame.cpp
    src/df_csv.cpp
    src/df_join.cpp
    src/df_window.cpp
    src/df_db.cpp
    src/df_parquet.cpp
)

# Process QPP files
qore_wrap_qpp_value(DF_QPP_SOURCES DOXLIST _dox_src METALIST DF_QPP_META_SRC ${DF_QPP_SRC})
add_custom_target(DF_QPP_GENERATED_FILES DEPENDS qpp ${DF_QPP_SOURCES})

# Create the module library
set(df_module_name "dataframe")

add_library(${df_module_name} MODULE ${DF_CPP_SRC} ${DF_QPP_SOURCES})
target_link_libraries(${df_module_name} Eigen3::Eigen)
target_link_libraries(${df_module_name} ${QORE_ARROW_TARGETS})
target_include_directories(${df_module_name} PRIVATE ${LLVM_INCLUDE_DIRS})
add_dependencies(${df_module_name} libqore DF_QPP_GENERATED_FILES)

qore_binary_module_qore(${df_module_name} "${VERSION}" "/${VERSION}")
qore_install_qpp_metadata(${df_module_name} ${DF_QPP_META_SRC})
