# Copyright (C) 2026 Qore Technologies, s.r.o.
cmake_minimum_required(VERSION 3.16)
project(QoreConstructorCleanupProbe LANGUAGES CXX)
find_package(Qore REQUIRED)
add_library(ctorprobe MODULE ctorprobe.cpp)
target_compile_features(ctorprobe PRIVATE cxx_std_17)
target_include_directories(ctorprobe PRIVATE ${QORE_INCLUDE_DIRS})
target_link_libraries(ctorprobe PRIVATE ${QORE_LIBRARY})
set_target_properties(ctorprobe PROPERTIES PREFIX "" SUFFIX ".qmod")
