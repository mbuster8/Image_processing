# Everything except main(), so the viewer's tests can link it.
add_library(viewer_ui STATIC
    activity_log.cpp
    activity_log.h
    activity_log_panel.cpp
    activity_log_panel.h
    data_folder.cpp
    data_folder.h
    display_text.cpp
    display_text.h
    divider_splitter.cpp
    divider_splitter.h
    eliding_label.cpp
    eliding_label.h
    epoch_time.h
    image_info_bar.cpp
    image_info_bar.h
    image_sidebar.cpp
    image_sidebar.h
    image_store.cpp
    image_store.h
    image_view.cpp
    image_view.h
    main_window.cpp
    main_window.h
    main_window.ui
    private_files.cpp
    private_files.h
    qt_image_sink.cpp
    qt_image_sink.h
    server_controls.cpp
    server_controls.h
    storage_controller.cpp
    storage_controller.h
    storage_worker.cpp
    storage_worker.h
    thumbnail_action.h
    thumbnail_delegate.cpp
    thumbnail_delegate.h
    thumbnail_model.cpp
    thumbnail_model.h
    thumbnail_panel.cpp
    thumbnail_panel.h)
target_include_directories(viewer_ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(viewer_ui
    PUBLIC
        Qt6::Widgets
        qt_common
        viewer_core::viewer_core
        visual_debug::visual_debug
    PRIVATE
        opencv_imgproc)
image_processing_set_warnings(viewer_ui)

# Resources belong to the executable: a static library's resources are dropped by the linker unless
# initialised by hand.
qt_add_executable(visual_debug_viewer MACOSX_BUNDLE
    main.cpp
    resources.qrc)
set_target_properties(visual_debug_viewer PROPERTIES
    MACOSX_BUNDLE_BUNDLE_NAME "Visual Debug Viewer"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.proximasus.visual-debug-viewer"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    WIN32_EXECUTABLE ON)
target_compile_definitions(visual_debug_viewer PRIVATE IMAGE_PROCESSING_VERSION="${PROJECT_VERSION}")
target_link_libraries(visual_debug_viewer PRIVATE viewer_ui)
image_processing_set_warnings(visual_debug_viewer)
image_processing_add_sanitizer_defaults(visual_debug_viewer)
