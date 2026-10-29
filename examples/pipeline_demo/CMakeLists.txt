# The stages as a library, so the unit tests can check them without sending anything.
add_library(pipeline STATIC
    pipeline.cpp
    pipeline.h)
target_include_directories(pipeline PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pipeline PUBLIC opencv_core PRIVATE opencv_imgproc)
id_vision_lab_set_warnings(pipeline)

add_executable(pipeline_demo main.cpp)
target_link_libraries(pipeline_demo PRIVATE pipeline opencv_imgcodecs visual_debug::visual_debug)
id_vision_lab_set_warnings(pipeline_demo)
id_vision_lab_add_sanitizer_defaults(pipeline_demo)
