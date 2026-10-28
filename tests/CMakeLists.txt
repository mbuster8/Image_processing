# One GoogleTest executable per area. Each test case runs in its own process, and the timeout turns a hung
# socket into a failure instead of a stall. Each program also runs once whole, as IDEs such as Qt Creator run
# it: state one test leaves behind breaks the next only when they share a process. Discovery runs the new
# binary once at build time; antivirus scanning can make that first launch slow, hence the longer timeout.
# Callers pass GTest::gtest_main, or GTest::gtest with their own main().
function(image_processing_add_test_executable target)
    cmake_parse_arguments(PARSE_ARGV 1 ARG "" "" "SOURCES;LIBRARIES")
    add_executable(${target} ${ARG_SOURCES})
    target_link_libraries(${target} PRIVATE ${ARG_LIBRARIES})
    image_processing_set_warnings(${target})
    image_processing_add_sanitizer_defaults(${target})
    gtest_discover_tests(${target} DISCOVERY_TIMEOUT 30 PROPERTIES TIMEOUT 60)
    add_test(NAME "${target} (whole program)" COMMAND ${target})
    set_tests_properties("${target} (whole program)" PROPERTIES LABELS whole TIMEOUT 120)
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
        viewer_core/save_name_test.cpp
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
            image_processing/activity_log_panel_test.cpp
            image_processing/activity_log_test.cpp
            image_processing/appearance_test.cpp
            image_processing/data_folder_test.cpp
            image_processing/image_file_test.cpp
            image_processing/image_info_bar_test.cpp
            image_processing/image_sidebar_test.cpp
            image_processing/image_store_test.cpp
            image_processing/image_view_test.cpp
            image_processing/main_window_test.cpp
            image_processing/outline_icons_test.cpp
            image_processing/qimage_conversion_test.cpp
            image_processing/qt_image_sink_test.cpp
            image_processing/server_controls_test.cpp
            image_processing/shades_test.cpp
            image_processing/storage_worker_test.cpp
            image_processing/thumbnail_panel_test.cpp
        LIBRARIES
            image_processing_ui
            opencv_imgcodecs
            Qt6::Test
            GTest::gtest)
    # A program of its own: the client and the viewer each have a main_window.h.
    image_processing_add_test_executable(client_ui_tests
        SOURCES
            image_processing/test_main.cpp
            client/main_window_test.cpp
            client/send_counter_test.cpp
        LIBRARIES
            client_ui
            opencv_imgcodecs
            Qt6::Test
            GTest::gtest)
    # The shared Qt test helpers: test_main.cpp's settings and control_checks.h.
    target_include_directories(client_ui_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/image_processing)
endif()
