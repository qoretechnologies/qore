# Copyright 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: Apache-2.0
cmake_minimum_required(VERSION 3.20)
project(QoreOnnxPackageTest LANGUAGES CXX)
find_package(Protobuf REQUIRED)
find_package(ONNX 1.20 REQUIRED CONFIG)
add_executable(qore-onnx-test link.cpp)
target_link_libraries(qore-onnx-test PRIVATE ONNX::onnx ONNX::onnx_proto)
