add_library(viewer_core STATIC
    src/display_converter.cpp
    src/image_history.cpp
    src/pixel_inspector.cpp
    src/viewport.cpp)
add_library(viewer_core::viewer_core ALIAS viewer_core)

target_include_directories(viewer_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_compile_features(viewer_core PUBLIC cxx_std_20)

# The UI-free half of the viewer: OpenCV only, so it builds and tests without Qt.
target_link_libraries(viewer_core
    PUBLIC
        opencv_core
    PRIVATE
        opencv_imgproc)

image_processing_set_warnings(viewer_core)
