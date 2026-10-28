# Everything except main(), so the example sender's tests can link it.
add_library(example_sender_ui STATIC
    main_window.cpp
    main_window.h
    main_window.ui
    send_counter.h)
target_include_directories(example_sender_ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(example_sender_ui
    PUBLIC
        Qt6::Widgets
        qt_common
        visual_debug::visual_debug
    PRIVATE
        viewer_core::viewer_core)
image_processing_set_warnings(example_sender_ui)

# Resources belong to the executable: a static library's resources are dropped by the linker unless
# initialised by hand.
qt_add_executable(visual_debug_example_sender MACOSX_BUNDLE
    main.cpp
    resources.qrc)
set_target_properties(visual_debug_example_sender PROPERTIES
    MACOSX_BUNDLE_BUNDLE_NAME "Visual Debug Example Sender"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.proximasus.visual-debug-example-sender"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    WIN32_EXECUTABLE ON)
target_compile_definitions(visual_debug_example_sender PRIVATE IMAGE_PROCESSING_VERSION="${PROJECT_VERSION}")
target_link_libraries(visual_debug_example_sender PRIVATE example_sender_ui)
image_processing_add_bundle_icon(visual_debug_example_sender
                                 "${PROJECT_SOURCE_DIR}/resources/icons/example_sender_icon.png")
image_processing_set_warnings(visual_debug_example_sender)
image_processing_add_sanitizer_defaults(visual_debug_example_sender)
