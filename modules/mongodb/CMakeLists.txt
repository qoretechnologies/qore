# MongoDB module CMake configuration
#
# Copyright (C) 2025 - 2026 Qore Technologies, s.r.o.
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

# Find libmongoc and libbson
# Prefer pkg-config as it works reliably with shared libs only (Alpine's cmake
# config references static libs that may not be installed)
# mongo-c-driver 2.x renamed pkg-config files:
#   libmongoc-1.0 -> mongoc2, libbson-1.0 -> bson2
find_package(PkgConfig QUIET)
if (PkgConfig_FOUND)
    # Try 2.x names first
    pkg_check_modules(MONGOC mongoc2)
    pkg_check_modules(BSON bson2)
    if (NOT MONGOC_FOUND OR NOT BSON_FOUND)
        # Fall back to 1.x names
        pkg_check_modules(MONGOC libmongoc-1.0)
        pkg_check_modules(BSON libbson-1.0)
    endif()
endif()

if (MONGOC_FOUND AND BSON_FOUND)
    set(MONGODB_INCLUDE_DIRS ${MONGOC_INCLUDE_DIRS} ${BSON_INCLUDE_DIRS})
    set(MONGODB_LIBRARY_DIRS ${MONGOC_LIBRARY_DIRS} ${BSON_LIBRARY_DIRS})
    set(MONGODB_LIBRARIES ${MONGOC_LIBRARIES} ${BSON_LIBRARIES})
else()
    # Fall back to cmake config
    # mongo-c-driver 2.x: package "mongoc" with target mongoc::mongoc
    # mongo-c-driver 1.x: package "mongoc-1.0" with target mongo::mongoc_shared
    find_package(mongoc QUIET)
    if (mongoc_FOUND)
        set(MONGODB_INCLUDE_DIRS)
        set(MONGODB_LIBRARIES mongoc::mongoc)
    else()
        find_package(mongoc-1.0 QUIET)
        if (mongoc-1.0_FOUND)
            set(MONGODB_INCLUDE_DIRS)
            set(MONGODB_LIBRARIES mongo::mongoc_shared)
        else()
            message(STATUS "MongoDB C driver not found - mongodb module will not be built")
            return()
        endif()
    endif()
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
include_directories(${CMAKE_SOURCE_DIR}/build/include)
include_directories(${CMAKE_SOURCE_DIR}/include)
if (MONGODB_INCLUDE_DIRS)
    include_directories(${MONGODB_INCLUDE_DIRS})
endif()
if (MONGODB_LIBRARY_DIRS)
    link_directories(${MONGODB_LIBRARY_DIRS})
endif()

# QPP source files (Qore PreProcessor)
set(MONGODB_QPP_SRC
    src/QC_MongoClient.qpp
    src/QC_MongoDatabase.qpp
    src/QC_MongoCollection.qpp
    src/QC_MongoCursor.qpp
    src/QC_ObjectId.qpp
    src/ql_mongodb.qpp
)

# C++ source files
set(MONGODB_CPP_SRC
    src/mongodb-module.cpp
    src/bson_conversion.cpp
    src/MongoClient.cpp
    src/MongoDatabase.cpp
    src/MongoCollection.cpp
    src/MongoCursor.cpp
    src/ObjectId.cpp
    src/QoreMongoStream.cpp
)

set(QORE_DOX_MONGODB_TMPL_SRC
    docs/mainpage.dox.tmpl
)

# Process QPP files
qore_wrap_qpp_value(MONGODB_QPP_SOURCES DOXLIST _dox_src ${MONGODB_QPP_SRC})
add_custom_target(MONGODB_QPP_GENERATED_FILES DEPENDS qpp ${MONGODB_QPP_SOURCES})

# Create the module library
set(mongodb_module_name "mongodb")

add_library(${mongodb_module_name} MODULE ${MONGODB_CPP_SRC} ${MONGODB_QPP_SOURCES})

target_link_libraries(${mongodb_module_name} ${MONGODB_LIBRARIES})
add_dependencies(${mongodb_module_name} libqore MONGODB_QPP_GENERATED_FILES)
set(MODULE_DOX_INPUT ${CMAKE_BINARY_DIR}/modules/mongodb/mainpage.dox ${_dox_src})
qore_binary_module_qore(${mongodb_module_name} "${VERSION}" "/${VERSION}")

if (DOXYGEN_FOUND)
    # Add MongoDB logo SVG to the module docs (reuse the logo from MongoDbDataProvider)
    file(APPEND ${CMAKE_BINARY_DIR}/modules/mongodb/Doxyfile
        "\nHTML_EXTRA_FILES       += \"${CMAKE_SOURCE_DIR}/qlib/MongoDbDataProvider/mongodb-logo.svg\"\n")
    qore_wrap_dox(QORE_DOX_MONGODB_SRC ${QORE_DOX_MONGODB_TMPL_SRC})
    add_custom_target(QORE_MONGODB_MOD_DOX_FILES DEPENDS ${QORE_DOX_MONGODB_SRC})
    add_dependencies(docs-mongodb QORE_MONGODB_MOD_DOX_FILES)
endif()
