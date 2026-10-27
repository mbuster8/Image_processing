add_library(qt_common STATIC
    image_file.cpp
    image_file.h
    qimage_conversion.cpp
    qimage_conversion.h)

target_include_directories(qt_common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(qt_common
    PUBLIC
        Qt6::Widgets
        opencv_core
    PRIVATE
        opencv_imgcodecs
        visual_debug::visual_debug)

image_processing_set_warnings(qt_common)
