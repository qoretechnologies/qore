# Copyright (C) 2026 Qore Technologies, s.r.o.
cmake_minimum_required(VERSION 3.16)
project(QoreRuntimeBuiltinSurvivor LANGUAGES CXX)
find_package(Qore REQUIRED)
add_library(rtsurv MODULE rtsurv.cpp)
target_compile_features(rtsurv PRIVATE cxx_std_17)
target_include_directories(rtsurv PRIVATE ${QORE_INCLUDE_DIRS})
target_link_libraries(rtsurv PRIVATE ${QORE_LIBRARY})
set_target_properties(rtsurv PROPERTIES PREFIX "" SUFFIX ".qmod")
