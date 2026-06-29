# Copyright (c) 2026 Alexey.Khoraskin@gmail.com
# Licensed under the Apache License, Version 2.0
# https://github.com/alexeykhoraskin/MyTools/tree/main/pdf2md
cmake_minimum_required(VERSION 3.20)
project(pdf2md VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(POPPLER REQUIRED poppler-cpp)

find_library(HPDF_LIBRARY hpdf-2.3.0 REQUIRED)
find_path(HPDF_INCLUDE_DIR hpdf.h REQUIRED)

add_executable(pdf2md src/main.cpp)

target_compile_definitions(pdf2md PRIVATE
    PROJECT_VERSION="${PROJECT_VERSION}"
)

target_include_directories(pdf2md PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${POPPLER_INCLUDE_DIRS}
    ${HPDF_INCLUDE_DIR}
)

target_link_libraries(pdf2md PRIVATE
    ${POPPLER_LIBRARIES}
    ${HPDF_LIBRARY}
)
