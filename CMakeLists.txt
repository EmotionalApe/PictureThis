cmake_minimum_required(VERSION 3.21)

project(PictureThis
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

qt_add_executable(PictureThis
    src/main.cpp

    src/media/ProcessRunner.cpp
    src/media/ProcessRunner.h

    src/media/MediaService.cpp
    src/media/MediaService.h
)
target_link_libraries(PictureThis PRIVATE
    Qt6::Widgets
)