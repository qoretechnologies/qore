# Copyright 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: Apache-2.0
cmake_minimum_required(VERSION 3.25)
project(ArrowInstalledSDK LANGUAGES CXX)
find_package(Arrow 25 CONFIG REQUIRED)
find_package(ArrowCompute CONFIG REQUIRED)
find_package(ArrowAcero CONFIG REQUIRED)
find_package(ArrowDataset CONFIG REQUIRED)
find_package(ArrowFlight CONFIG REQUIRED)
find_package(ArrowFlightSql CONFIG REQUIRED)
find_package(Parquet CONFIG REQUIRED)
find_package(Threads REQUIRED)
add_executable(arrow-sdk sdk.cpp)
target_compile_features(arrow-sdk PRIVATE cxx_std_20)
target_link_libraries(arrow-sdk PRIVATE
    Arrow::arrow_shared ArrowCompute::arrow_compute_shared
    ArrowAcero::arrow_acero_shared ArrowDataset::arrow_dataset_shared
    ArrowFlight::arrow_flight_shared ArrowFlightSql::arrow_flight_sql_shared
    Parquet::parquet_shared Threads::Threads)
