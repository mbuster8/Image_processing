add_library(qt_common STATIC
    about.cpp
    about.h
    appearance.cpp
    appearance.h
    application_identity.cpp
    application_identity.h
    controls.cpp
    controls.h
    image_file.cpp
    image_file.h
    outline_icons.cpp
    outline_icons.h
    qimage_conversion.cpp
    qimage_conversion.h
    shades.cpp
    shades.h)

target_include_directories(qt_common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(qt_common
    PUBLIC
        Qt6::Widgets
        opencv_core
    PRIVATE
        opencv_imgcodecs
        viewer_core::viewer_core
        visual_debug::visual_debug)

id_vision_lab_set_warnings(qt_common)
