# Everything but the reading of cameras and files as a library, so the unit tests can check it without a
# camera. Only the executable needs OpenCV's videoio module.
add_library(live_sender_lib STATIC
    live_sender.cpp
    live_sender.h)
target_include_directories(live_sender_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(live_sender_lib PUBLIC opencv_core visual_debug::visual_debug PRIVATE opencv_imgproc)
id_vision_lab_set_warnings(live_sender_lib)

find_package(OpenCV REQUIRED COMPONENTS videoio)
add_executable(live_sender main.cpp)
target_link_libraries(live_sender PRIVATE live_sender_lib opencv_videoio)
id_vision_lab_set_warnings(live_sender)
id_vision_lab_add_sanitizer_defaults(live_sender)
