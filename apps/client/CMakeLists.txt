qt_add_executable(client MACOSX_BUNDLE
    main.cpp
    main_window.cpp
    main_window.h
    main_window.ui)
set_target_properties(client PROPERTIES
    MACOSX_BUNDLE_BUNDLE_NAME "Image Processing Client"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    WIN32_EXECUTABLE ON)
target_link_libraries(client PRIVATE
    Qt6::Widgets
    qt_common
    viewer_core::viewer_core
    visual_debug::visual_debug)
image_processing_set_warnings(client)
image_processing_add_sanitizer_defaults(client)
