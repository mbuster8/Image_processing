# Everything except main(), so the client's tests can link it.
add_library(client_ui STATIC
    main_window.cpp
    main_window.h
    main_window.ui
    send_counter.h)
target_include_directories(client_ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(client_ui
    PUBLIC
        Qt6::Widgets
        qt_common
        visual_debug::visual_debug
    PRIVATE
        viewer_core::viewer_core)
image_processing_set_warnings(client_ui)

qt_add_executable(client MACOSX_BUNDLE
    main.cpp)
set_target_properties(client PROPERTIES
    MACOSX_BUNDLE_BUNDLE_NAME "Image Processing Client"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    WIN32_EXECUTABLE ON)
target_link_libraries(client PRIVATE client_ui)
image_processing_set_warnings(client)
image_processing_add_sanitizer_defaults(client)
