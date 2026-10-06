# Copyright (c) 2026 Alexey.Khoraskin@gmail.com
# Licensed under the Apache License, Version 2.0
# https://github.com/alexeykhoraskin/MyTools/tree/main/pdf2md
cmake_minimum_required(VERSION 3.20)
project(pdf2md VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(PDF2MD_BUILD_TESTS "Build unit tests" ON)

if(PDF2MD_BUILD_TESTS)
    enable_testing()
    find_package(GTest REQUIRED)
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(POPPLER REQUIRED IMPORTED_TARGET poppler-cpp)

find_library(HPDF_LIBRARY NAMES hpdf hpdf-2.3.0 REQUIRED)
find_path(HPDF_INCLUDE_DIR hpdf.h REQUIRED)

set(PDF2MD_WARNINGS -Wall -Wextra -Wpedantic)

# ── core library: unicode/text + markdown parsing, no external deps ──

add_library(pdf2md_core STATIC
    src/pdf_classify.cpp
    src/md_parse.cpp
)
target_include_directories(pdf2md_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_compile_options(pdf2md_core PRIVATE ${PDF2MD_WARNINGS})

# ── poppler backend: PDF text extraction ──

add_library(pdf2md_poppler STATIC
    src/pdf_extract.cpp
)
target_link_libraries(pdf2md_poppler PUBLIC pdf2md_core PkgConfig::POPPLER)
target_compile_options(pdf2md_poppler PRIVATE ${PDF2MD_WARNINGS})

# ── libharu backend: Markdown -> PDF rendering ──

add_library(pdf2md_haru STATIC
    src/md_render.cpp
)
target_include_directories(pdf2md_haru PRIVATE ${HPDF_INCLUDE_DIR})
target_link_libraries(pdf2md_haru PUBLIC pdf2md_core ${HPDF_LIBRARY})
target_compile_options(pdf2md_haru PRIVATE ${PDF2MD_WARNINGS})

# ── conversion pipeline (ties both backends together) ──

add_library(pdf2md_converter STATIC
    src/converter.cpp
)
target_link_libraries(pdf2md_converter PUBLIC
    pdf2md_poppler
    pdf2md_haru
)
target_compile_options(pdf2md_converter PRIVATE ${PDF2MD_WARNINGS})

# ── executable ──

add_executable(pdf2md src/main.cpp)
target_compile_definitions(pdf2md PRIVATE
    PROJECT_VERSION="${PROJECT_VERSION}"
)
target_link_libraries(pdf2md PRIVATE pdf2md_converter)
target_compile_options(pdf2md PRIVATE ${PDF2MD_WARNINGS})

install(TARGETS pdf2md RUNTIME DESTINATION bin)

# ── tests ──

if(PDF2MD_BUILD_TESTS)
    foreach(test_name text_utils pdf_classify md_parse)
        add_executable(test_${test_name} tests/test_${test_name}.cpp)
        target_link_libraries(test_${test_name} PRIVATE pdf2md_core GTest::gtest GTest::gtest_main)
        target_compile_options(test_${test_name} PRIVATE ${PDF2MD_WARNINGS})
        include(GoogleTest)
        gtest_discover_tests(test_${test_name})
    endforeach()
endif()