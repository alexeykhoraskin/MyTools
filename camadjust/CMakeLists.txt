# Copyright (c) 2026 Alexey.Khoraskin@gmail.com
# Licensed under the Apache License, Version 2.0
# https://github.com/alexeykhoraskin/MyTools/tree/main/camadjust
cmake_minimum_required(VERSION 3.20)
project(camadjust VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)
find_package(OpenCV REQUIRED)

add_executable(camadjust
    src/main.cpp
    src/CameraProcessor.cpp
    src/V4L2Output.cpp
    src/PresetManager.cpp
)

target_include_directories(camadjust PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(camadjust PRIVATE
    Qt6::Widgets
    ${OpenCV_LIBS}
)

target_compile_definitions(camadjust PRIVATE
    PROJECT_VERSION="${PROJECT_VERSION}"
)
