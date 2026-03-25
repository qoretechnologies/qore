# ML module CMake configuration
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

# Eigen is required (checked in top-level CMakeLists.txt)
# Try with version constraint first (Eigen 3.4.x), then without (Eigen 5.x)
find_package(Eigen3 3.4 QUIET NO_MODULE)
if (NOT Eigen3_FOUND)
    find_package(Eigen3 REQUIRED NO_MODULE)
endif()

# ONNX Runtime is optional
set(HAVE_ONNXRUNTIME FALSE)
find_package(PkgConfig QUIET)
if (PkgConfig_FOUND)
    # Also check /opt/onnxruntime for pkg-config files
    set(ENV{PKG_CONFIG_PATH} "$ENV{PKG_CONFIG_PATH}:/opt/onnxruntime/lib/pkgconfig")
    pkg_check_modules(ONNXRUNTIME QUIET libonnxruntime)
endif()
if (NOT ONNXRUNTIME_FOUND)
    find_library(ONNXRUNTIME_LIBRARY NAMES onnxruntime
                 HINTS /opt/onnxruntime/lib)
    find_path(ONNXRUNTIME_INCLUDE_DIR onnxruntime_cxx_api.h
              PATH_SUFFIXES onnxruntime
              HINTS /opt/onnxruntime/include)
    if (ONNXRUNTIME_LIBRARY AND ONNXRUNTIME_INCLUDE_DIR)
        set(ONNXRUNTIME_FOUND TRUE)
        set(ONNXRUNTIME_LIBRARIES ${ONNXRUNTIME_LIBRARY})
        set(ONNXRUNTIME_INCLUDE_DIRS ${ONNXRUNTIME_INCLUDE_DIR})
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
include_directories(${CMAKE_BINARY_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/include)

# QPP source files
set(ML_QPP_SRC
    src/QC_IsolationForest.qpp
    src/QC_DBSCAN.qpp
    src/QC_KMeans.qpp
    src/QC_HoltWinters.qpp
    src/QC_PCA.qpp
    src/QC_SeasonalDecomposition.qpp
    src/QC_LinearRegression.qpp
    src/QC_LogisticRegression.qpp
    src/QC_LOF.qpp
    src/QC_KNN.qpp
    src/QC_GMM.qpp
    src/QC_OnnxModel.qpp
    src/QC_StandardScaler.qpp
    src/QC_MinMaxScaler.qpp
    src/QC_Imputer.qpp
    src/QC_MLPipeline.qpp
    src/QC_CrossValidator.qpp
    src/QC_DecisionTree.qpp
    src/ql_ml.qpp
)

# C++ source files
set(ML_CPP_SRC
    src/ml-module.cpp
    src/ml_common.cpp
    src/ml_serialization.cpp
    src/IsolationForest.cpp
    src/DBSCAN.cpp
    src/KMeans.cpp
    src/HoltWinters.cpp
    src/PCA.cpp
    src/SeasonalDecomposition.cpp
    src/LinearRegression.cpp
    src/LogisticRegression.cpp
    src/LOF.cpp
    src/KNN.cpp
    src/GMM.cpp
    src/StandardScaler.cpp
    src/MinMaxScaler.cpp
    src/Imputer.cpp
    src/MLPipeline.cpp
    src/ml_metrics.cpp
    src/CrossValidator.cpp
    src/FlatTree.cpp
    src/DecisionTree.cpp
)

# Conditional ONNX Runtime support
if (ONNXRUNTIME_FOUND)
    message(STATUS "ONNX Runtime found - building with ONNX model support")
    add_definitions(-DHAVE_ONNXRUNTIME)
    set(HAVE_ONNXRUNTIME TRUE)
    list(APPEND ML_CPP_SRC src/OnnxModel.cpp)
else()
    message(STATUS "ONNX Runtime not found - OnnxModel class will throw MISSING-FEATURE-ERROR")
endif()

set(QORE_DOX_ML_TMPL_SRC
    docs/mainpage.dox.tmpl
)

# Process QPP files
qore_wrap_qpp_value(ML_QPP_SOURCES DOXLIST _dox_src ${ML_QPP_SRC})
add_custom_target(ML_QPP_GENERATED_FILES DEPENDS qpp ${ML_QPP_SOURCES})

# Create the module library
set(ml_module_name "ml")

add_library(${ml_module_name} MODULE ${ML_CPP_SRC} ${ML_QPP_SOURCES})
target_link_libraries(${ml_module_name} Eigen3::Eigen)
if (HAVE_ONNXRUNTIME)
    target_include_directories(${ml_module_name} PRIVATE ${ONNXRUNTIME_INCLUDE_DIRS})
    if (ONNXRUNTIME_LIBRARY_DIRS)
        target_link_directories(${ml_module_name} PRIVATE ${ONNXRUNTIME_LIBRARY_DIRS})
    endif()
    target_link_libraries(${ml_module_name} ${ONNXRUNTIME_LIBRARIES})
endif()
add_dependencies(${ml_module_name} libqore ML_QPP_GENERATED_FILES)

set(MODULE_DOX_INPUT ${CMAKE_BINARY_DIR}/modules/ml/mainpage.dox ${_dox_src})
qore_binary_module_qore(${ml_module_name} "${VERSION}" "/${VERSION}")

if (DOXYGEN_FOUND)
    qore_wrap_dox(QORE_DOX_ML_SRC ${QORE_DOX_ML_TMPL_SRC})
    add_custom_target(QORE_ML_MOD_DOX_FILES DEPENDS ${QORE_DOX_ML_SRC})
    add_dependencies(docs-ml QORE_ML_MOD_DOX_FILES)
endif()
