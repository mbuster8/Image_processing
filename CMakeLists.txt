cmake_minimum_required(VERSION 3.25)

project(ImageProcessing
    VERSION 2.0.0
    DESCRIPTION "Visual debugger for OpenCV images: a library sends cv::Mat over ZeroMQ to a Qt viewer"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(IMAGE_PROCESSING_BUILD_QT_APPS "Build the Qt 6 viewer and demo client" ON)
option(IMAGE_PROCESSING_BUILD_EXAMPLES "Build the command-line examples" ON)
option(IMAGE_PROCESSING_BUILD_TESTS "Build the GoogleTest unit tests" ON)
option(IMAGE_PROCESSING_SANITIZERS "Build with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

include(cmake/compiler_warnings.cmake)
include(cmake/sanitizers.cmake)

find_package(OpenCV REQUIRED COMPONENTS core imgproc imgcodecs)
find_package(cppzmq REQUIRED)
find_package(nlohmann_json 3.11 REQUIRED)

add_subdirectory(libs/visual_debug)
add_subdirectory(libs/viewer_core)

if(IMAGE_PROCESSING_BUILD_QT_APPS)
    add_subdirectory(apps)
endif()

if(IMAGE_PROCESSING_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(IMAGE_PROCESSING_BUILD_TESTS)
    enable_testing()
    find_package(GTest REQUIRED)
    include(GoogleTest)
    add_subdirectory(tests)
endif()
