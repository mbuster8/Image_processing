# One GoogleTest executable per area. Each test runs in its own process, and the timeout turns a hung socket
# into a failure instead of a stall. Callers pass GTest::gtest_main, or GTest::gtest with their own main().
function(image_processing_add_test_executable target)
    cmake_parse_arguments(PARSE_ARGV 1 ARG "" "" "SOURCES;LIBRARIES")
    add_executable(${target} ${ARG_SOURCES})
    target_link_libraries(${target} PRIVATE ${ARG_LIBRARIES})
    image_processing_set_warnings(${target})
    image_processing_add_sanitizer_defaults(${target})
    gtest_discover_tests(${target} PROPERTIES TIMEOUT 60)
endfunction()

image_processing_add_test_executable(visual_debug_tests
    SOURCES
        visual_debug/wire_format_test.cpp
        visual_debug/endpoint_test.cpp
        visual_debug/client_server_test.cpp
        visual_debug/send_image_test.cpp
    LIBRARIES
        visual_debug::visual_debug
        cppzmq
        nlohmann_json::nlohmann_json
        GTest::gtest_main)

image_processing_add_test_executable(viewer_core_tests
    SOURCES
        viewer_core/display_converter_test.cpp
        viewer_core/image_history_test.cpp
        viewer_core/pixel_inspector_test.cpp
        viewer_core/viewport_test.cpp
    LIBRARIES
        viewer_core::viewer_core
        GTest::gtest_main)

if(IMAGE_PROCESSING_BUILD_EXAMPLES)
    image_processing_add_test_executable(pipeline_tests
        SOURCES
            examples/pipeline_test.cpp
        LIBRARIES
            pipeline
            visual_debug::visual_debug
            GTest::gtest_main)
endif()

# The thin Qt helpers, with an offscreen QApplication; the windows themselves are checked end to end.
if(IMAGE_PROCESSING_BUILD_QT_APPS)
    find_package(Qt6 REQUIRED COMPONENTS Widgets Test)
    image_processing_add_test_executable(qt_ui_tests
        SOURCES
            image_processing/test_main.cpp
            image_processing/image_file_test.cpp
            image_processing/image_view_test.cpp
            image_processing/qimage_conversion_test.cpp
            image_processing/qt_image_sink_test.cpp
            image_processing/thumbnail_panel_test.cpp
        LIBRARIES
            image_processing_ui
            opencv_imgcodecs
            Qt6::Test
            GTest::gtest)
endif()
