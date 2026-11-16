cmake_minimum_required(VERSION 3.21)

project(PictureThis
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)

# Compiler warnings
if (MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Sanitizer build option
option(ENABLE_ASAN "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
if (ENABLE_ASAN)
    if (MSVC)
        add_compile_options(/fsanitize=address)
    else()
        add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address,undefined)
    endif()
endif()

find_package(Qt6 REQUIRED COMPONENTS Widgets)

qt_add_executable(PictureThis
    src/main.cpp

    src/ui/MainWindow.cpp
    src/ui/MainWindow.h
    src/ui/ImageAudioWidget.cpp
    src/ui/ImageAudioWidget.h
    src/ui/RangeTimelineWidget.cpp
    src/ui/RangeTimelineWidget.h
    src/ui/VideoTrimWidget.cpp
    src/ui/VideoTrimWidget.h
    src/ui/VideoCombineWidget.cpp
    src/ui/VideoCombineWidget.h
    src/ui/VideoCompressWidget.cpp
    src/ui/VideoCompressWidget.h
    src/ui/VideoCompareWidget.cpp
    src/ui/VideoCompareWidget.h
    src/ui/SettingsDialog.cpp
    src/ui/SettingsDialog.h

    src/media/ProcessRunner.cpp
    src/media/ProcessRunner.h
    src/media/ExecutableLocator.cpp
    src/media/ExecutableLocator.h

    src/media/InspectionTypes.h
    src/media/VideoMetadata.h
    src/media/InspectionWorker.cpp
    src/media/InspectionWorker.h
    src/media/InspectionScheduler.cpp
    src/media/InspectionScheduler.h
    src/media/MediaCache.h
    src/media/MediaCache.cpp

    src/media/MediaService.cpp
    src/media/MediaService.h
)
target_link_libraries(PictureThis PRIVATE
    Qt6::Widgets
)

# Automated testing suite
enable_testing()
add_subdirectory(tests)