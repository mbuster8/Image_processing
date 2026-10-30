# One GoogleTest executable per area. Each test case runs in its own process, and the timeout turns a hung
# socket into a failure instead of a stall. Each program also runs once whole, as IDEs such as Qt Creator run
# it: state one test leaves behind breaks the next only when they share a process. Discovery runs the new
# binary once at build time; antivirus scanning can make that first launch slow, hence the longer timeout.
# Callers pass GTest::gtest_main, or GTest::gtest with their own main().
function(id_vision_lab_add_test_executable target)
    cmake_parse_arguments(PARSE_ARGV 1 ARG "" "" "SOURCES;LIBRARIES")
    add_executable(${target} ${ARG_SOURCES})
    target_link_libraries(${target} PRIVATE ${ARG_LIBRARIES})
    id_vision_lab_set_warnings(${target})
    id_vision_lab_add_sanitizer_defaults(${target})
    gtest_discover_tests(${target} DISCOVERY_TIMEOUT 30 PROPERTIES TIMEOUT 60)
    add_test(NAME "${target} (whole program)" COMMAND ${target})
    set_tests_properties("${target} (whole program)" PROPERTIES LABELS whole TIMEOUT 120)
endfunction()

id_vision_lab_add_test_executable(visual_debug_tests
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

id_vision_lab_add_test_executable(viewer_core_tests
    SOURCES
        viewer_core/display_converter_test.cpp
        viewer_core/display_settings_test.cpp
        viewer_core/image_history_test.cpp
        viewer_core/live_channels_test.cpp
        viewer_core/pixel_inspector_test.cpp
        viewer_core/save_name_test.cpp
        viewer_core/viewport_test.cpp
    LIBRARIES
        viewer_core::viewer_core
        opencv_imgproc
        GTest::gtest_main)

if(ID_VISION_LAB_BUILD_EXAMPLES)
    id_vision_lab_add_test_executable(pipeline_tests
        SOURCES
            examples/pipeline_test.cpp
        LIBRARIES
            pipeline
            visual_debug::visual_debug
            GTest::gtest_main)
    id_vision_lab_add_test_executable(live_sender_tests
        SOURCES
            examples/live_sender_test.cpp
        LIBRARIES
            live_sender_lib
            visual_debug::visual_debug
            cppzmq
            nlohmann_json::nlohmann_json
            GTest::gtest_main)
    # test_support.h: the collecting sink the delivery test uses.
    target_include_directories(live_sender_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/visual_debug)
endif()

# The Qt code, with an offscreen QApplication. Every Qt test program uses qt_common/test_main.cpp, which keeps
# the tests' settings apart from the owner's. The apps get a program each: both have a main_window.h.
if(ID_VISION_LAB_BUILD_QT_APPS)
    find_package(Qt6 REQUIRED COMPONENTS Widgets Test)
    id_vision_lab_add_test_executable(qt_common_tests
        SOURCES
            qt_common/test_main.cpp
            qt_common/about_test.cpp
            qt_common/appearance_test.cpp
            qt_common/image_file_test.cpp
            qt_common/outline_icons_test.cpp
            qt_common/qimage_conversion_test.cpp
            qt_common/shades_test.cpp
        LIBRARIES
            qt_common
            viewer_core::viewer_core
            opencv_imgcodecs
            Qt6::Test
            GTest::gtest)
    id_vision_lab_add_test_executable(viewer_ui_tests
        SOURCES
            qt_common/test_main.cpp
            viewer/activity_log_panel_test.cpp
            viewer/activity_log_test.cpp
            viewer/data_folder_test.cpp
            viewer/image_info_bar_test.cpp
            viewer/image_sidebar_test.cpp
            viewer/image_store_test.cpp
            viewer/image_view_test.cpp
            viewer/live_channel_panel_test.cpp
            viewer/main_window_test.cpp
            viewer/qt_image_sink_test.cpp
            viewer/server_controls_test.cpp
            viewer/storage_worker_test.cpp
            viewer/thumbnail_panel_test.cpp
        LIBRARIES
            viewer_ui
            opencv_imgcodecs
            Qt6::Test
            GTest::gtest)
    id_vision_lab_add_test_executable(example_sender_ui_tests
        SOURCES
            qt_common/test_main.cpp
            example_sender/main_window_test.cpp
            example_sender/send_counter_test.cpp
        LIBRARIES
            example_sender_ui
            opencv_imgcodecs
            Qt6::Test
            GTest::gtest)
    # The shared Qt test helpers, such as palette_support.h and control_checks.h.
    target_include_directories(viewer_ui_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/qt_common)
    target_include_directories(example_sender_ui_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/qt_common)
endif()
