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

# Apache Arrow/Parquet is optional
set(HAVE_ARROW FALSE)
find_package(Arrow QUIET)
find_package(Parquet QUIET)
if (Arrow_FOUND AND Parquet_FOUND)
    set(HAVE_ARROW TRUE)
    message(STATUS "Apache Arrow/Parquet found - building with Parquet I/O support")
    add_definitions(-DHAVE_ARROW)
else()
    message(STATUS "Apache Arrow/Parquet not found - readParquet/writeParquet will "
        "throw MISSING-FEATURE-ERROR")
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
    src/QC_GroupedDataFrame.qpp
    src/ql_dataframe.qpp
)

# C++ source files
set(DF_CPP_SRC
    src/dataframe-module.cpp
    src/df_column.cpp
    src/DataFrame.cpp
    src/GroupedDataFrame.cpp
    src/df_csv.cpp
    src/df_join.cpp
    src/df_window.cpp
)

# Conditional Arrow/Parquet support
if (HAVE_ARROW)
    list(APPEND DF_CPP_SRC src/df_parquet.cpp)
endif()

# Process QPP files
qore_wrap_qpp_value(DF_QPP_SOURCES DOXLIST _dox_src ${DF_QPP_SRC})
add_custom_target(DF_QPP_GENERATED_FILES DEPENDS qpp ${DF_QPP_SOURCES})

# Create the module library
set(df_module_name "dataframe")

add_library(${df_module_name} MODULE ${DF_CPP_SRC} ${DF_QPP_SOURCES})
target_link_libraries(${df_module_name} Eigen3::Eigen)
if (HAVE_ARROW)
    target_link_libraries(${df_module_name} Arrow::arrow_shared Parquet::parquet_shared)
endif()
add_dependencies(${df_module_name} libqore DF_QPP_GENERATED_FILES)

qore_binary_module_qore(${df_module_name} "${VERSION}" "/${VERSION}")
